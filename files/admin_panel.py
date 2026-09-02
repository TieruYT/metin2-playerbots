#!/usr/bin/env python3
# =============================================================
# Metin2 Admin Panel v2 - built for non-technical users
# Config: /usr/local/etc/m2panel.conf  (M2PANEL_CONF to move it)
# =============================================================
import datetime, json, os, random, re, socket, sys, threading, time, hashlib, hmac, secrets, sqlite3, subprocess
import urllib.request, urllib.error, urllib.parse
from functools import wraps
from flask import Flask, request, session, redirect, url_for, render_template_string, flash, send_file, jsonify
from flask import send_from_directory
from flask import has_request_context
from flask.sessions import SecureCookieSessionInterface
# markupsafe is Jinja2's own escaping library and therefore already installed
# wherever Flask is -- it is not a new dependency. The patch-log page needs it
# to turn a Markdown file fetched over the network into something that can be
# put on a page without it being able to put anything ON the page.
from markupsafe import escape, Markup
import pymysql

# The game stores quest states as numeric indices. 557528158 is the engine's
# stable index for the generated ``__complete`` state (the same value is used
# by every compiled quest). Keeping the canonical mission order here lets the
# panel describe the exact same progression that playerbot_manager.cpp runs.
BIOLOGIST_COMPLETE_STATE = 557528158
BIOLOGIST_MISSIONS = (
    ("make_herb_lv4", 4, "Kwiat Brzoskwini", 5),
    ("make_herb_lv7", 7, "Pokrzywa", 5),
    ("make_herb_lv10", 10, "Kwiat Kaki", 5),
    ("make_herb_lv15", 15, "Korzeń Gango", 5),
    ("make_herb_lv20", 20, "Bez", 10),
    ("make_herb_lv25", 25, "Grzyb Tue", 10),
)

# The official ``special.levelup_quest`` choices for the M1/M2 stage.  The
# game server writes progress to quest ``levelup``; the panel only interprets
# those canonical flags and never maintains a second counter.
HUNTING_MISSIONS = {
    2: ((171, "Hungry Stray Dog", 10), (172, "Hungry Wolf", 5)),
    3: ((171, "Hungry Stray Dog", 20), (172, "Hungry Wolf", 10)),
    4: ((172, "Hungry Wolf", 15), (173, "Hungry Alpha Wolf", 5)),
    5: ((173, "Hungry Alpha Wolf", 10), (174, "Hungry Blue Wolf", 10)),
    6: ((174, "Hungry Blue Wolf", 20), (178, "Hungry Wild Boar", 10)),
    7: ((178, "Hungry Wild Boar", 10), (175, "Hungry Alpha Blue Wolf", 5)),
    8: ((178, "Hungry Wild Boar", 20), (175, "Hungry Alpha Blue Wolf", 10)),
    9: ((175, "Hungry Alpha Blue Wolf", 15), (179, "Hungry Red Boar", 5)),
    10: ((175, "Hungry Alpha Blue Wolf", 20), (179, "Hungry Red Boar", 10)),
    11: ((179, "Hungry Red Boar", 10), (180, "Hungry Bear", 5)),
    12: ((180, "Hungry Bear", 15), (176, "Hungry Grey Wolf", 10)),
    13: ((176, "Hungry Grey Wolf", 20), (181, "Hungry Grizzly", 5)),
    14: ((181, "Hungry Grizzly", 15), (177, "Hungry Alpha Grey Wolf", 5)),
    15: ((181, "Hungry Grizzly", 20), (177, "Hungry Alpha Grey Wolf", 10)),
    16: ((177, "Hungry Alpha Grey Wolf", 15), (184, "Hungry Tiger", 5)),
    17: ((177, "Hungry Alpha Grey Wolf", 20), (184, "Hungry Tiger", 10)),
    18: ((184, "Hungry Tiger", 10), (182, "Hungry Black Bear", 10)),
    19: ((182, "Hungry Black Bear", 20), (183, "Hungry Brown Bear", 10)),
    20: ((183, "Hungry Brown Bear", 20), (352, "Craven White Oath Archer", 15)),
    21: ((352, "Craven White Oath Archer", 20), (185, "Hungry White Tiger", 10)),
    22: ((185, "Hungry White Tiger", 25), (354, "Craven White Oath Commander", 10)),
    23: ((354, "Craven White Oath Commander", 20), (451, "Evil Black Storm Soldier", 40)),
    24: ((451, "Evil Black Storm Soldier", 60), (402, "Black Wind Maniac", 80)),
    25: ((551, "Strong Savage Infantry", 80), (454, "Evil Black Storm Joh-Hwan", 20)),
}

HUNTING_MOB_NAMES_PL = {
    171: "Głodny Dziki Pies", 172: "Głodny Wilk", 173: "Głodny Alfa Wilk",
    174: "Głodny Niebieski Wilk", 175: "Głodny Alfa Niebieski Wilk",
    176: "Głodny Szary Wilk", 177: "Głodny Alfa Szary Wilk",
    178: "Głodny Dzik", 179: "Głodny Czerwony Dzik",
    180: "Głodny Niedźwiedź", 181: "Głodny Grizzly",
    182: "Głodny Czarny Niedźwiedź", 183: "Głodny Brązowy Niedźwiedź",
    184: "Głodny Tygrys", 185: "Głodny Biały Tygrys",
    352: "Tchórzliwy Łucznik Białych Zaprzysiężonych",
    354: "Tchórzliwy Dowódca Białych Zaprzysiężonych",
    402: "Maniak Czarnego Wiatru", 451: "Żołnierz Czarnego Wiatru",
    454: "Joh-Hwan Czarnego Wiatru", 551: "Silny Dziki Piechur",
}

def hunting_progress_label(current, selection, remain, complete, language=None):
    language = language or (lang() if has_request_context() else "en")
    current, selection = int(current or 0), 2 if int(selection or 1) == 2 else 1
    remain, complete = max(0, int(remain or 0)), max(0, int(complete or 0))
    mission = HUNTING_MISSIONS.get(current)
    if mission:
        mob_vnum, mob_name, required = mission[selection - 1]
        if language == "pl":
            mob_name = HUNTING_MOB_NAMES_PL.get(mob_vnum, mob_name)
        done = max(0, required - remain)
        return "Lv %d • %s: %d/%d" % (current, mob_name, done, required)
    if complete:
        return ("Ukończone do Lv %d" if language == "pl" else
                "Completed through Lv %d") % complete
    return "Jeszcze nierozpoczęte" if language == "pl" else "Not started yet"

# Party membership is intentionally restricted by the game core to this
# deterministic ten-percent cohort.  Runtime parties are not persisted in the
# database, so the live map exposes eligibility consistently with the server
# instead of the old, purely decorative ``pid % 3`` estimate.
BOT_PARTY_MODULO = 10
BOT_PARTY_REMAINDER = 3


def bot_in_party_cohort(pid):
    return int(pid or 0) % BOT_PARTY_MODULO == BOT_PARTY_REMAINDER


PLAYERBOT_STATUS_PATHS = (
    "/opt/metin2/var/channel1/first/playerbot_status.tsv",
    "/opt/metin2/var/channel1/game1/playerbot_status.tsv",
    "/opt/metin2/var/channel1/game2/playerbot_status.tsv",
)
_PLAYERBOT_STATUS_LOCK = threading.Lock()
_PLAYERBOT_STATUS_CACHE_KEY = None
_PLAYERBOT_STATUS_CACHE = {}

BOT_PERSONALITY_LABELS = {
    "pl": {
        0: "Wytrwały poszukiwacz", 1: "Pogromca Metinów", 2: "Towarzysz drużyny",
        3: "Mistrz ekwipunku", 4: "Rozważny zbieracz", 5: "Wędrowiec",
    },
    "en": {
        0: "Steady adventurer", 1: "Metin breaker", 2: "Team companion",
        3: "Gear specialist", 4: "Careful collector", 5: "Wanderer",
    },
}
BOT_AMBITION_LABELS = {
    "pl": {
        0: "Poziom", 1: "Ekwipunek", 2: "Metiny", 3: "Koń",
        4: "Biolog", 5: "Umiejętności",
    },
    "en": {
        0: "Level", 1: "Equipment", 2: "Metins", 3: "Horse",
        4: "Biologist", 5: "Skills",
    },
}
BOT_GOAL_LABELS = {
    "pl": {
        0: "Zdobywanie poziomu", 1: "Przetrwanie", 2: "Wybór profesji",
        3: "Zdobycie ekwipunku", 4: "Uzupełnienie zapasów", 5: "Ulepszanie EQ",
        6: "Rozwój umiejętności", 7: "Polowanie na Metiny", 8: "Silne cele w PT",
        9: "Misja Biologa", 10: "Misja Polowania", 11: "Rozwój konia",
    },
    "en": {
        0: "Gain levels", 1: "Survive", 2: "Choose profession",
        3: "Get equipment", 4: "Restock", 5: "Refine equipment",
        6: "Improve skills", 7: "Hunt Metins", 8: "Party challenges",
        9: "Biologist mission", 10: "Hunting mission", 11: "Develop horse",
    },
}
BOT_ACTION_LABELS = {
    "pl": {
        0: "Planuje następny ruch", 1: "Podróżuje", 2: "Walczy", 3: "Podnosi łup",
        4: "Regeneruje się", 5: "Wybiera profesję", 6: "Handluje", 7: "Ulepsza EQ",
        8: "Czyta KU", 9: "Wkłada KD", 10: "Organizuje PT", 11: "Robi Biologa",
        12: "Odwiedza Stajennego",
    },
    "en": {
        0: "Planning next move", 1: "Travelling", 2: "Fighting", 3: "Picking up loot",
        4: "Recovering", 5: "Choosing profession", 6: "Trading", 7: "Refining gear",
        8: "Reading a skill book", 9: "Socketing a spirit stone", 10: "Organising a party",
        11: "Doing Biologist mission", 12: "Visiting the Stable Boy",
    },
}


def read_playerbot_live_status():
    """Return the newest atomic status snapshots written by all game cores."""
    global _PLAYERBOT_STATUS_CACHE_KEY, _PLAYERBOT_STATUS_CACHE
    now = time.time()
    cache_key = tuple(
        (path, os.path.getmtime(path) if os.path.exists(path) else 0)
        for path in PLAYERBOT_STATUS_PATHS
    )
    with _PLAYERBOT_STATUS_LOCK:
        if cache_key == _PLAYERBOT_STATUS_CACHE_KEY:
            return dict(_PLAYERBOT_STATUS_CACHE)

        result = {}
        for path, modified in cache_key:
            if not modified or now - modified > 20:
                continue
            try:
                # The r40250 core and Polish locale tables use Windows-1250.
                with open(path, "r", encoding="cp1250", errors="replace") as stream:
                    for line in stream:
                        if line.startswith("pid\t"):
                            continue
                        parts = line.rstrip("\r\n").split("\t", 13)
                        if len(parts) != 14:
                            continue
                        values = [int(value) for value in parts[:13]]
                        pid = values[0]
                        result[pid] = {
                            "pid": pid, "personality_id": values[1],
                            "ambition_id": values[2], "role": values[3],
                            "in_pt": bool(values[4]), "goal_id": values[5],
                            "action_id": values[6], "updated_ms": values[7],
                            "map_index": values[8], "x": values[9], "y": values[10],
                            "hp": values[11], "max_hp": values[12],
                            "status": parts[13],
                        }
            except (OSError, ValueError):
                continue

        _PLAYERBOT_STATUS_CACHE_KEY = cache_key
        _PLAYERBOT_STATUS_CACHE = result
        return dict(result)


def localize_playerbot_status(entry, language):
    if not entry:
        return None
    if language == "pl":
        return entry.get("status") or BOT_ACTION_LABELS["pl"].get(
            entry.get("action_id"), BOT_ACTION_LABELS["pl"][0])
    text = entry.get("status") or ""
    replacements = (
        ("Nieprzytomny - czekam na wstanie", "Unconscious - waiting to revive"),
        ("Odpoczywam po smierci", "Recovering after death"),
        ("Uciekam - mam malo HP", "Retreating - low HP"),
        ("Rozbijam ", "Breaking "), ("Walcze z ", "Fighting "),
        ("Gonie ", "Chasing "), ("Podnosze lup", "Picking up loot"),
        ("Regeneruje HP", "Recovering HP"),
        ("Szukam przeciwnika", "Looking for a target"),
        ("Szukam miejsca do expa", "Looking for a levelling spot"),
        ("Ulepszam ekwipunek", "Refining equipment"),
    )
    for source, target in replacements:
        text = text.replace(source, target)
    return text or BOT_ACTION_LABELS["en"].get(
        entry.get("action_id"), BOT_ACTION_LABELS["en"][0])


def playerbot_live_labels(entry, language):
    language = language if language in ("pl", "en") else "en"
    if not entry:
        return {
            "personality": BOT_PERSONALITY_LABELS[language][0],
            "ambition": BOT_AMBITION_LABELS[language][0],
            "goal": BOT_GOAL_LABELS[language][0],
            "action": BOT_ACTION_LABELS[language][0],
        }
    return {
        "personality": BOT_PERSONALITY_LABELS[language].get(
            entry.get("personality_id"), BOT_PERSONALITY_LABELS[language][0]),
        "ambition": BOT_AMBITION_LABELS[language].get(
            entry.get("ambition_id"), BOT_AMBITION_LABELS[language][0]),
        "goal": BOT_GOAL_LABELS[language].get(
            entry.get("goal_id"), BOT_GOAL_LABELS[language][0]),
        "action": localize_playerbot_status(entry, language),
    }


SKILL_GROUP_NAMES = {
    (0, 1): "BODY", (0, 2): "MENTAL",
    (1, 1): "Sztylety", (1, 2): "Łucznik",
    (2, 1): "Magia Broni", (2, 2): "Czarna Magia",
    (3, 1): "Smok", (3, 2): "Leczenie",
}

SKILL_GROUP_NAMES_EN = {
    (0, 1): "Body", (0, 2): "Mental",
    (1, 1): "Dagger", (1, 2): "Archer",
    (2, 1): "Weaponry", (2, 2): "Black Magic",
    (3, 1): "Dragon", (3, 2): "Healing",
}

PLAYER_SKILLS = {
    (0, 1): ((1, "Trzystronne Cięcie"), (2, "Wir Miecza"),
             (3, "Berserk"), (4, "Aura Miecza"), (5, "Szarża")),
    (0, 2): ((16, "Duchowe Uderzenie"), (17, "Tąpnięcie"),
             (18, "Uderzenie Miecza"), (19, "Silne Ciało"), (20, "Walnięcie")),
    (1, 1): ((31, "Zasadzka"), (32, "Szybki Atak"),
             (33, "Wirujący Sztylet"), (34, "Krycie się"), (35, "Trująca Chmura")),
    (1, 2): ((46, "Powtarzalny Strzał"), (47, "Deszcz Strzał"),
             (48, "Ognista Strzała"), (49, "Bezszelestny Chód"), (50, "Trująca Strzała")),
    (2, 1): ((61, "Uderzenie Palcem"), (62, "Smoczy Wir"),
             (63, "Czarowane Ostrze"), (64, "Strach"),
             (65, "Czarowana Zbroja"), (66, "Rozproszenie Magii")),
    (2, 2): ((76, "Mroczne Uderzenie"), (77, "Ogniste Uderzenie"),
             (78, "Ognisty Duch"), (79, "Mroczna Ochrona"),
             (80, "Duchowy Cios"), (81, "Mroczna Sfera")),
    (3, 1): ((91, "Latający Talizman"), (92, "Strzelający Smok"),
             (93, "Smoczy Skowyt"), (94, "Błogosławieństwo"),
             (95, "Odbicie"), (96, "Pomoc Smoka")),
    (3, 2): ((106, "Błyskawiczny Rzut"), (107, "Przywołanie Błyskawicy"),
             (108, "Burzowy Szpon"), (109, "Leczenie"),
             (110, "Zwinność"), (111, "Zwiększenie Ataku")),
}

PLAYER_SKILLS_EN = {
    (0, 1): ((1, "Three-Way Cut"), (2, "Sword Spin"), (3, "Berserk"),
             (4, "Aura of Sword"), (5, "Dash")),
    (0, 2): ((16, "Spirit Strike"), (17, "Bash"), (18, "Stump"),
             (19, "Strong Body"), (20, "Sword Strike")),
    (1, 1): ((31, "Ambush"), (32, "Fast Attack"), (33, "Rolling Dagger"),
             (34, "Stealth"), (35, "Poison Cloud")),
    (1, 2): ((46, "Repetitive Shot"), (47, "Arrow Shower"),
             (48, "Fire Arrow"), (49, "Feather Walk"), (50, "Poison Arrow")),
    (2, 1): ((61, "Finger Strike"), (62, "Dragon Swirl"),
             (63, "Enchanted Blade"), (64, "Fear"),
             (65, "Enchanted Armour"), (66, "Dispel")),
    (2, 2): ((76, "Dark Strike"), (77, "Flame Strike"),
             (78, "Flame Spirit"), (79, "Dark Protection"),
             (80, "Spirit Strike"), (81, "Dark Orb")),
    (3, 1): ((91, "Flying Talisman"), (92, "Shooting Dragon"),
             (93, "Dragon Roar"), (94, "Blessing"),
             (95, "Reflect"), (96, "Dragon's Aid")),
    (3, 2): ((106, "Lightning Throw"), (107, "Summon Lightning"),
             (108, "Lightning Claw"), (109, "Cure"),
             (110, "Swiftness"), (111, "Attack Up")),
}


def skill_rank_label(master_type, level):
    master_type, level = int(master_type or 0), int(level or 0)
    if master_type >= 3 or level >= 40:
        return "P"
    if master_type == 2 or level >= 30:
        return "G%d" % max(1, level - 29)
    if master_type == 1 or level >= 20:
        return "M%d" % max(1, level - 19)
    return str(level)


def parse_player_skills(raw, job, skill_group, language=None):
    """Decode r40250's packed TPlayerSkill[255] (master, level, next-read)."""
    if isinstance(raw, memoryview):
        raw = raw.tobytes()
    elif isinstance(raw, str):
        raw = raw.encode("latin-1", "ignore")
    raw = raw or b""
    base_job = int(job or 0) % 4
    group = int(skill_group or 0)
    result = []
    skill_names = PLAYER_SKILLS if language == "pl" else PLAYER_SKILLS_EN
    for vnum, name in skill_names.get((base_job, group), ()):
        offset = vnum * 6
        master_type = raw[offset] if offset < len(raw) else 0
        level = raw[offset + 1] if offset + 1 < len(raw) else 0
        result.append({
            "vnum": vnum,
            "name": name,
            "level": level,
            "master_type": master_type,
            "rank": skill_rank_label(master_type, level),
        })
    return result

# ---- where everything lives -------------------------------------------------
# The panel was written for a FreeBSD box where install.sh puts its files in
# /usr/local/m2panel and its config in /usr/local/etc. Those are still the
# defaults, so an existing installation keeps working with no changes at all.
# Inside a container none of those paths necessarily exist, so every one of
# them can be moved with an environment variable. Set M2PANEL_DIR alone and
# all four files below follow it; set an individual variable to place just
# that one file somewhere else.
def _env_path(name, default):
    """Value of environment variable 'name', or 'default' when it is unset/empty."""
    return os.environ.get(name, "").strip() or default

PANEL_DIR  = _env_path("M2PANEL_DIR", "/usr/local/m2panel")
# Files shipped next to admin_panel.py itself; that is where install.sh puts
# them and where they sit in the source tree, so this needs no variable to
# work — but a read-only image may want them elsewhere.
_HERE      = os.path.dirname(os.path.abspath(__file__))

CLIENT_ZIP = _env_path("M2PANEL_CLIENT_ZIP", os.path.join(PANEL_DIR, "client.zip"))

# ---- client download quota --------------------------------------------------
# The client is 1.2 GB; a bot fetching it in a loop would saturate the uplink
# for everyone. Each address gets DL_MAX full downloads per sliding 24 hours,
# tracked in a small SQLite file so the count survives panel restarts. Resumed
# downloads (Range requests that start mid-file) and HEAD probes are free -
# only a fresh fetch of the whole file spends a slot.
DL_DB  = _env_path("M2PANEL_DL_DB", os.path.join(PANEL_DIR, "downloads.db"))
DL_MAX, DL_WINDOW = 3, 24 * 3600

# A second ceiling, for the whole server rather than one address. The per-address
# limit assumes an abuser has one address; a botnet, an open proxy pool or a
# shared NAT breaks that assumption, and 1.2 GB a time turns into real money on
# a metered link long before anyone notices. This caps the total, so the worst
# case is bounded no matter how many addresses show up.
DL_DAY_MAX = int(os.environ.get("M2PANEL_DL_DAY_MAX", "100") or 100)

# ---- live server status -----------------------------------------------------
# Shown on the front page: is the game up, and how many people are in it.
# "Up" means both doors are open: the login server AND the first channel are
# listening. Read out of the machine's socket table rather than by making a
# test connection, because the game binds to the public address only - a
# loopback connect always fails.
# The player count is the number of established outside connections to the
# channel ports; each connected client keeps exactly one, and the cores
# talking to each other (loopback / own address) are ignored. Cached for 30
# seconds so a busy front page cannot hammer the game.
#
# Which tool reads that socket table differs per system:
#   FreeBSD  sockstat -4l / -4c   (the original, still the default there)
#   Linux    ss -H -4 -n -l -t / ss -H -4 -n -t state established
# Both are tried in turn, so the same file works on either without being told
# which it is on. M2PANEL_STATUS_CMD forces one of them ("sockstat" / "ss")
# when a machine happens to have both and picks the wrong one.
#
# In a container the panel usually cannot see the game's sockets at all —
# different network namespace, and neither tool would report anything. Set
# M2PANEL_STATUS_HOST to the game's host name and the status is decided by
# opening a TCP connection to it instead. That answers "is it up?" honestly;
# it cannot count players (nobody outside the game's namespace can), so the
# count then stays at 0.
GAME_PORT_LOW, GAME_PORT_HIGH = 13000, 13099
_SRV = {"ts": 0.0, "up": False, "count": 0}

STATUS_CMD  = _env_path("M2PANEL_STATUS_CMD", "auto").lower()

# Resolved lazily, not at import: the environment variable wins, but the config
# file is the fallback -- and CONF is not loaded until much further down this
# file. The container entrypoint writes the game's host name into the config as
# "game_host"; without this fallback nothing ever read it, the panel dropped
# through to listing local sockets, and a containerised panel -- which is in a
# different network namespace from the game -- reported a healthy server as
# offline. On FreeBSD there is no "game_host" key, so this stays empty and the
# socket listing is used exactly as before.
def _status_host():
    return _env_path("M2PANEL_STATUS_HOST", "") or str(CONF.get("game_host", "") or "")

def _status_ports():
    try:
        return [int(p) for p in CONF.get("status_ports", [11000, 13000])]
    except (TypeError, ValueError):
        return [11000, 13000]

def _sockets_sockstat(listening):
    """FreeBSD. Returns [(local_endpoint, foreign_endpoint), ...]; foreign is
    "" for listening sockets. Columns: USER COMMAND PID FD PROTO LOCAL FOREIGN."""
    out = subprocess.run(["sockstat", "-4l" if listening else "-4c"],
                         capture_output=True, text=True, timeout=3).stdout
    rows = []
    for line in out.splitlines():
        f = line.split()
        if len(f) >= 6 and ":" in f[5]:
            rows.append((f[5], f[6] if len(f) >= 7 and ":" in f[6] else ""))
    return rows

def _sockets_ss(listening):
    """Linux. Same return shape as _sockets_sockstat.

    -H drops the header so there is nothing to skip, -n keeps ports numeric
    (otherwise 11002 would come back as a service name). Columns of the
    remaining line: STATE RECV-Q SEND-Q LOCAL PEER."""
    cmd = ["ss", "-H", "-4", "-n", "-t"]
    cmd += ["-l"] if listening else ["state", "established"]
    out = subprocess.run(cmd, capture_output=True, text=True, timeout=3).stdout
    rows = []
    for line in out.splitlines():
        f = line.split()
        # 'ss -l' still prints the state column, 'ss state established' does
        # not - so count from the right, where local and peer always sit.
        if len(f) < 4 or ":" not in f[-2] or ":" not in f[-1]:
            continue
        rows.append((f[-2], f[-1]))
    return rows

def _sockets(listening):
    """The socket table, read with whichever tool this machine has."""
    order = {"sockstat": (_sockets_sockstat,),
             "ss":       (_sockets_ss,)}.get(STATUS_CMD,
                         (_sockets_sockstat, _sockets_ss))
    last = None
    for reader in order:
        try:
            return reader(listening)
        except (OSError, subprocess.SubprocessError) as e:
            last = e            # not installed on this system - try the next
    raise last or OSError("no socket listing tool available")

def _status_by_connect(host):
    """Container mode: is something accepting connections on every status port
    of the game host? Cannot count players, so that stays 0."""
    for p in _status_ports():
        try:
            with socket.create_connection((host, p), timeout=2):
                pass
        except OSError:
            return False, 0
    return True, 0

INGAME_WINDOW = 600      # seconds; see _players_in_game()

def _players_in_game():
    """How many characters the game has written to recently.

    Only used when the socket table is out of reach -- in a container the game's
    connections live in another network namespace, so counting them is simply
    not possible from here and the count used to sit at 0 forever. The game core
    saves each logged-in character every few minutes, so a recent `last_play' is
    a sound stand-in: it cannot see someone who logged in seconds ago, and it
    keeps counting someone for a few minutes after they leave. Approximate and
    honest beats exact and unavailable.
    """
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT COUNT(*) AS n FROM player.player "
                        "WHERE last_play > (NOW() - INTERVAL %s SECOND)",
                        (INGAME_WINDOW,))
            row = cur.fetchone()
        return int(row["n"] if isinstance(row, dict) else row[0])
    except Exception:
        return 0        # database unreachable: report nothing, never guess

_ACC = {"ts": 0.0, "any": True}

def accounts_exist():
    """Is there at least one account on this server?

    Drives the "you have not made an account yet" prompt on the front page. It
    defaults to True and stays True if the database cannot be reached: a server
    with a wobbly database should not greet everyone as a first-time visitor.
    Cached, because it is asked on every page render and the answer only ever
    changes once.
    """
    now = time.time()
    if now - _ACC["ts"] < 15:
        return _ACC["any"]
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT EXISTS(SELECT 1 FROM account.account) AS n")
            row = cur.fetchone()
        _ACC["any"] = bool(row["n"] if isinstance(row, dict) else row[0])
    except Exception:
        _ACC["any"] = True
    _ACC["ts"] = now
    return _ACC["any"]

_HELPER = {"ts": 0.0, "seen": None}

def ingame_helper_seen():
    """Has the in-game helper ever answered on this server?

    It is a quest running inside the game that watches web_admin_queue and
    carries out what the panel puts there. The Docker build installs it and the
    port carries the Lua binding it calls, so on a current stack it is there --
    but this file also runs on servers built before that, and on hand-assembled
    ones, where nothing ever picks a row up.

    Note what this cannot see. The quest waits 30 s before claiming a row for a
    player who is not logged in, and the panel gives up at 7 s and cancels, so
    an action aimed at somebody offline never leaves evidence either way. Only a
    successful in-game action does. Both messages that depend on this therefore
    say what happened rather than why, and are true whichever it is.

    Without this the panel could not tell "the player is not logged in" from
    "nobody is listening", and reported the first for both: it told operators
    their character had not been in game while they were standing in it.

    Evidence, not configuration: a row that ever reached any status other than
    pending or cancelled was moved by the helper, because nothing else touches
    them. Cached for a minute -- and once the answer is yes it cannot go back
    to no, so it is then never asked again.
    """
    if _HELPER["seen"]:
        return True
    now = time.time()
    if _HELPER["seen"] is not None and now - _HELPER["ts"] < 60:
        return _HELPER["seen"]
    _HELPER["ts"] = now
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT EXISTS(SELECT 1 FROM player.web_admin_queue "
                        "WHERE status NOT IN ('pending','cancelled')) AS n")
            row = cur.fetchone()
        _HELPER["seen"] = bool(row["n"] if isinstance(row, dict) else row[0])
    except Exception:
        # Cannot tell: claim nothing, and let the wording stay as it was.
        _HELPER["seen"] = True
    return _HELPER["seen"]

def server_status():
    now = time.time()
    if now - _SRV["ts"] < 30:
        return _SRV
    up, count = False, 0
    try:
        status_host = _status_host()
        if status_host:
            up, count = _status_by_connect(status_host)
            if up:
                count = _players_in_game()
        else:
            ports_open = set()
            for local, _foreign in _sockets(listening=True):
                port = local.rpartition(":")[2]
                if port.isdigit():
                    ports_open.add(int(port))
            up = all(p in ports_open for p in _status_ports())

            if up:
                seen = set()
                for local, foreign in _sockets(listening=False):
                    if not foreign:
                        continue
                    local_ip, _, local_port = local.rpartition(":")
                    foreign_ip = foreign.rpartition(":")[0]
                    if not local_port.isdigit():
                        continue
                    if not (GAME_PORT_LOW <= int(local_port) <= GAME_PORT_HIGH):
                        continue
                    if foreign_ip in ("127.0.0.1", local_ip):
                        continue
                    seen.add(foreign)
                count = len(seen)
    except Exception:
        # no way to read the socket table, or it failed - claim nothing rather
        # than guess; the page then simply shows the server as down
        up, count = False, 0
    _SRV.update(ts=now, up=up, count=count)
    return _SRV

# ---- public server rates ----------------------------------------------------
# The front page shows the rates every private-server visitor asks about
# first. Missing table or unreachable database simply hides the badges -
# the front page must never break because the game side is down.
_RATES_PUB = {"ts": 0.0, "vals": None}

def public_rates():
    now = time.time()
    if now - _RATES_PUB["ts"] < 60:
        return _RATES_PUB["vals"]
    vals = None
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT name, value FROM player.web_admin_rates")
            found = {r["name"]: int(r["value"]) for r in cur.fetchall()}
        vals = {n: found.get(n, 100) for n in RATE_NAMES}
    except Exception:
        vals = None
    _RATES_PUB.update(ts=now, vals=vals)
    return vals

# ---- client download facts --------------------------------------------------
# Size is instant; the SHA256 of 1.2 GB takes a few seconds, so it is
# computed once in a background thread after startup and appears on the
# page as soon as it is ready.
CLIENT_FACTS = {"size": 0, "sha256": "", "mtime": 0.0, "ts": 0.0}
_CLIENT_LOCK = threading.Lock()

def _client_sha_worker(path, size, mtime):
    try:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
    except OSError:
        return
    with _CLIENT_LOCK:
        # Publish only if the file is still the one we hashed. A rebuild that
        # lands mid-hash would otherwise get the previous client's checksum.
        if CLIENT_FACTS["size"] == size and CLIENT_FACTS["mtime"] == mtime:
            CLIENT_FACTS["sha256"] = h.hexdigest()

def client_facts():
    """Size and checksum of the client, re-checked as it appears or changes.

    Reading this once at import was wrong in the normal case rather than an
    edge case: the client is built AFTER the panel starts -- often half an hour
    after, on a fresh install -- and it is rebuilt whenever the server address
    changes. The size and checksum then stayed at their startup values until
    somebody restarted the panel, and nothing anywhere told them to.
    """
    now = time.time()
    with _CLIENT_LOCK:
        if now - CLIENT_FACTS["ts"] < 10:      # a stat per request is wasteful
            return CLIENT_FACTS
        CLIENT_FACTS["ts"] = now
        try:
            st = os.stat(CLIENT_ZIP)
        except OSError:                        # not built yet, or removed again
            CLIENT_FACTS.update(size=0, sha256="", mtime=0.0)
            return CLIENT_FACTS
        if st.st_size == CLIENT_FACTS["size"] and st.st_mtime == CLIENT_FACTS["mtime"]:
            return CLIENT_FACTS
        CLIENT_FACTS.update(size=st.st_size, mtime=st.st_mtime, sha256="")
        size, mtime = st.st_size, st.st_mtime
    # Hashing 1.2 GB takes seconds, so it happens off the request thread and
    # the checksum simply appears on the page once it is ready.
    threading.Thread(target=_client_sha_worker,
                     args=(CLIENT_ZIP, size, mtime), daemon=True).start()
    return CLIENT_FACTS

def human_size(n):
    n = float(n)
    for unit in ("B", "KB", "MB"):
        if n < 1024:
            return "%.0f %s" % (n, unit)
        n /= 1024.0
    return "%.1f GB" % n
ITEMS_PATH = _env_path("M2PANEL_ITEMS", os.path.join(_HERE, "items.json"))

# ---- server-wide rates ------------------------------------------------------
# The installer puts these two next to the panel. apply_rates.sh reads the wanted
# rates out of player.web_admin_rates, hands them to the server-files profile and
# restarts the game; rates.status is the one-line answer it leaves behind.
RATES_SCRIPT = _env_path("M2PANEL_RATES_SCRIPT", os.path.join(PANEL_DIR, "apply_rates.sh"))
RATES_STATUS = _env_path("M2PANEL_RATES_STATUS", os.path.join(PANEL_DIR, "rates.status"))
RATE_NAMES   = ("exp", "drop", "yang")
RATE_MIN, RATE_MAX = 1, 10000

# ---- game master ranks ------------------------------------------------------
# Granting a rank is one row in common.gmlist, but the game reads that table
# into memory when it boots and never looks at it again, so the row on its own
# changes nothing until the next restart. Telling the running server means
# reaching its admin socket, and the panel deliberately cannot: that socket
# accepts SHUTDOWN and DC, and this is the process facing the internet.
#
# So, the rates pattern once more (see the long note above UPDATE_APPLY): the
# panel drops a request in a directory the game container watches, and m2-gm on
# the other side does the one fixed thing it knows how to do. Nothing in the
# file tells it what to run -- there is one verb and it is spelled out in
# m2-gm's own source.
GM_SPOOL   = _env_path("M2PANEL_GM_SPOOL", "/opt/m2spool")
GM_REQUEST = os.path.join(GM_SPOOL, "gm.request")

# ---- the language the game speaks -------------------------------------------
# Not the language of THIS page -- that is the switcher at the top right, and it
# only changes what the panel says. This is what the SERVER says: quest text,
# system messages, and the item and mob names it hands out. The pack ships all
# of it in fifteen languages and the switch is four files; m2-lang in the game
# container does the swapping and restarts the cores, because they read those
# files only while they boot.
#
# Same spool, same shape as the rates and the game master ranks.
LANG_SPOOL   = _env_path("M2PANEL_LANG_SPOOL", "/opt/m2spool")
LANG_REQUEST = os.path.join(LANG_SPOOL, "lang.request")
LANG_STATUS  = os.path.join(LANG_SPOOL, "lang.status")

# The fifteen the reference server files carry, by the codes their file names
# use. Named in their own language, which is how a person looks for their own.
# m2-lang decides what is really on offer -- it checks that all four files exist
# for a code before offering it -- so this list is labels, not permission.
GAME_LANGS = [
    ("en", "English"),   ("de", "Deutsch"),    ("tr", "Türkçe"),
    ("fr", "Français"),  ("es", "Español"),    ("it", "Italiano"),
    ("pt", "Português"), ("nl", "Nederlands"), ("pl", "Polski"),
    ("ro", "Română"),    ("hu", "Magyar"),     ("cz", "Čeština"),
    ("gr", "Ελληνικά"),  ("dk", "Dansk"),      ("ru", "Русский"),
]
GAME_LANG_NAMES = dict(GAME_LANGS)

def lang_status():
    """The note m2-lang leaves behind. Empty when the game container has never
    written one -- an older image, or one that has not started yet."""
    out = {}
    try:
        with open(LANG_STATUS, encoding="utf-8", errors="replace") as f:
            for line in f:
                k, sep, v = line.partition("=")
                if sep:
                    out[k.strip()] = v.strip()
    except OSError:
        pass
    return out

def game_lang():
    """The language the game is in right now, as a code.

    English when nothing says otherwise: that is what the server files ship as
    and what an untouched server runs. Reported rather than guessed wherever
    m2-lang has spoken."""
    code = (lang_status().get("lang") or "").strip()
    return code if code in GAME_LANG_NAMES else "en"

def game_lang_name():
    return GAME_LANG_NAMES.get(game_lang(), game_lang())

def lang_ask_for(code):
    """Ask the game container to switch. True when the request was written --
    which is not the same as it having happened, and the caller must not say
    otherwise."""
    try:
        tmp = LANG_REQUEST + ".new"
        with open(tmp, "w", encoding="utf-8") as f:
            f.write("id=%d-%d\nlang=%s\ntime=%d\n"
                    % (int(time.time()), os.getpid(), code, int(time.time())))
        os.replace(tmp, LANG_REQUEST)
        return True
    except OSError:
        return False

def gm_ask_for_reload():
    """Ask the game container to re-read the GM list. True when the request was
    written -- which is not the same as it having happened yet, and the caller
    must not claim otherwise."""
    try:
        tmp = GM_REQUEST + ".new"
        with open(tmp, "w", encoding="utf-8") as f:
            f.write("id=%d-%d\ntime=%d\n" % (int(time.time()), os.getpid(), int(time.time())))
        os.replace(tmp, GM_REQUEST)
        return True
    except OSError:
        # No spool mounted, or read-only. Worth reporting to the operator, not
        # worth losing the row that was just written.
        return False

CONF_PATH = _env_path("M2PANEL_CONF", "/usr/local/etc/m2panel.conf")
REQUIRED_CONF = ("flask_secret", "db_user", "db_pass", "salt", "pass_hash")

# ---- the admin passphrase, when the operator changes it here ----------------
# The hash lives in m2panel.conf and the panel may rewrite it: the entrypoint
# chowns that file to the panel account before dropping to it. The salt is
# renewed at the same time -- there is no reason to carry an old one forward.
#
# The plaintext is also written to a file of its own, and that is a deliberate
# trade rather than an oversight. The installer prints the passphrase at the end
# of every run, and it cannot print what it cannot read; a hash is a hash. The
# installer already keeps the generated one in .env in the clear (root-owned,
# 0600), so this is not a new kind of secret on this machine -- but it is one a
# person chose, and people reuse those. Anyone who would rather no passphrase of
# theirs sat on a disk should keep the generated one and not use this form.
PASSPHRASE_FILE = _env_path("M2PANEL_PASSPHRASE_FILE",
                            os.path.join(os.path.dirname(CONF_PATH) or ".",
                                         "panel.passphrase"))
PASSPHRASE_MIN = 8

# Every setting in the config file can also come from the environment, which is
# how a container is normally fed its secrets: M2PANEL_DB_PASS for "db_pass",
# M2PANEL_FLASK_SECRET for "flask_secret", and so on. An environment variable
# wins over the file, and if it supplies everything REQUIRED_CONF asks for then
# there need not be a config file at all. Nothing is read from the environment
# unless it is set, so an existing installation behaves exactly as before.
ENV_CONF = ("flask_secret", "db_host", "db_user", "db_pass", "salt", "pass_hash",
            "bind", "port", "brand", "client_url", "client_name",
            "inventory_slots", "max_item_count", "max_level", "status_ports", "local_only",
            "contact_email", "update_check", "update_apply", "update_command",
            # The client that runs in a browser. Environment only in practice:
            # m2panel.conf is written once at first run and never again, so a
            # setting added later can only reach an existing install this way.
            "browser_play", "browser_dir", "bridge_port", "bridge_host",
            "browser_cache_mb",
            # Set by the installer when, and only when, it puts nginx in front.
            # See _LocalProxyFix.
            "trust_proxy")

# The settings that are a yes or a no. Written out rather than guessed from the
# value, so that "0" means off everywhere instead of meaning off in some places
# and "a non-empty string, therefore on" in others.
BOOL_CONF = ("local_only", "update_check", "update_apply", "browser_play",
             "trust_proxy")

def _conf_from_env():
    """The config keys the environment sets, already turned into the right type."""
    out = {}
    for key in ENV_CONF:
        raw = os.environ.get("M2PANEL_" + key.upper(), "").strip()
        if not raw:
            continue
        if key in ("port", "inventory_slots", "max_item_count", "max_level", "bridge_port",
                   "browser_cache_mb"):
            try:
                out[key] = int(raw)
            except ValueError:
                continue                      # nonsense value: let the default stand
        elif key in BOOL_CONF:
            # local_only: whether this server is reachable only from the machine
            # it runs on. It cannot be inferred from "bind": a Linux server
            # behind nginx also binds the panel to 127.0.0.1 while being
            # perfectly public, and guessing there would tell a working host
            # that nobody can reach it. So the installer states it, and the
            # default is "no".
            # update_check / update_apply: see the version block further down.
            out[key] = raw.lower() in ("1", "true", "yes", "on")
        elif key == "status_ports":
            # "11002,13000" -> [11002, 13000]
            ports = [p.strip() for p in raw.replace(";", ",").split(",") if p.strip()]
            if all(p.isdigit() for p in ports) and ports:
                out[key] = [int(p) for p in ports]
        else:
            out[key] = raw
    return out

def _conf_die(problem):
    """Config is broken -> explain it in plain words and stop (no ugly traceback)."""
    print("\n".join([
        "",
        "  ⚠️  The Metin2 Admin Panel cannot start.",
        "",
        "  %s" % problem,
        "  Config file: %s" % CONF_PATH,
        "",
        "  EN: Please run the panel installer again — it creates this file for you.",
        "  DE: Bitte führe das Installationsprogramm erneut aus — es legt diese Datei für dich an.",
        "  TR: Lütfen panel kurulumunu tekrar çalıştır — bu dosyayı senin için oluşturur.",
        "",
        "  (Every setting can also be given as an environment variable instead,",
        "   e.g. M2PANEL_DB_PASS — that is how the container image is configured.)",
        "",
    ]), file=sys.stderr)
    sys.exit(1)

_ENV_CONF_VALUES = _conf_from_env()
try:
    with open(CONF_PATH) as f:
        CONF = json.load(f)
except FileNotFoundError:
    # Only fatal when the environment is not carrying the settings itself.
    if not all(_ENV_CONF_VALUES.get(k) for k in REQUIRED_CONF):
        _conf_die("The config file does not exist yet.")
    CONF = {}
except json.JSONDecodeError as e:
    _conf_die("The config file is not valid JSON (%s)." % e)
except OSError as e:
    _conf_die("The config file could not be read (%s)." % e)

if not isinstance(CONF, dict):
    _conf_die("The config file must contain a JSON object like { \"db_user\": \"...\" }.")
CONF.update(_ENV_CONF_VALUES)          # the environment has the last word
_missing = [k for k in REQUIRED_CONF if not CONF.get(k)]
if _missing:
    _conf_die("These settings are missing or empty in the config file: %s." % ", ".join(_missing))

# How many inventory slots this server build has. Raising it is only safe if the
# server really has more inventory pages, so the default stays at one page (45).
try:
    INVENTORY_SLOTS = int(CONF.get("inventory_slots", 45))
except (TypeError, ValueError):
    INVENTORY_SLOTS = 45
if INVENTORY_SLOTS < 1:
    INVENTORY_SLOTS = 45

# The biggest stack the game can store, which is whatever player.item.count
# holds. That is NOT the same everywhere: some server files declare it
# SMALLINT UNSIGNED (65535), the [40250] reference files declare it TINYINT
# UNSIGNED (255). It matters, because MySQL outside strict mode does not
# complain about a too-large number — it quietly stores the largest one that
# fits, and the admin then wonders why "give 1000 potions" produced 255.
# The default stays at 65535 so nothing changes for the servers this was
# written on; set "max_item_count" in the config (or M2PANEL_MAX_ITEM_COUNT)
# to 255 on server files whose column is a TINYINT.
try:
    MAX_ITEM_COUNT = int(CONF.get("max_item_count", 65535))
except (TypeError, ValueError):
    MAX_ITEM_COUNT = 65535
if not (1 <= MAX_ITEM_COUNT <= 65535):
    MAX_ITEM_COUNT = 65535

# The highest level this server will accept, which is MAX_LEVEL in the game's
# CONFIG (gPlayerMaxLevel) and nothing to do with what the engine could do --
# that ceiling is 120, PLAYER_MAX_LEVEL_CONST in common/length.h.
#
# It has to be known here because the server does not argue about it. Setting a
# level goes through PointChange(POINT_LEVEL, ...), and in char.cpp that reads
#
#     if ((GetLevel() + amount) > gPlayerMaxLevel)
#         return;
#
# -- a silent return. The quest reports success, the panel repeats it, and the
# character stays where it was. Worse, pc_set_level hands out skill, sub-skill
# and stat points BEFORE it changes the level, so an out-of-range attempt keeps
# the points and loses the level. So the panel refuses these itself rather than
# offering a number the server will quietly drop on the floor.
#
# The default matches the stack's own M2_MAX_LEVEL default; the compose file
# feeds both from the same value, so raising one raises the other.
try:
    MAX_LEVEL = int(CONF.get("max_level", 120))
except (TypeError, ValueError):
    MAX_LEVEL = 120
if not (1 <= MAX_LEVEL <= 120):
    MAX_LEVEL = 120

# ---- what the game download is called -------------------------------------
# The installer writes "client_name" into the config from the chosen server-files
# pack, e.g. "40250 - Official 2014 Client (15 Languages)". Config files written
# by an older installer do not have the key: then everything stays as it was.
# path separators, the characters Windows refuses in a file name, and everything
# that would break the Content-Disposition header (quotes, semicolons, controls).
# Accented letters survive — "Türkçe Client.zip" is a perfectly good file name.
_UNSAFE_IN_FILENAME = re.compile(r"[\x00-\x1f\x7f/\\<>:\"'|?*%;,]+")

def _client_download_name(raw):
    """Turn a friendly client name into a safe file name ending in .zip."""
    name = _UNSAFE_IN_FILENAME.sub(" ", str(raw or ""))
    name = re.sub(r"\s+", " ", name).strip()           # collapse the gaps that leaves
    name = name.strip(". ")                            # no ".." and no hidden files
    if name.lower().endswith(".zip"):                  # do not end up with "x.zip.zip"
        name = name[:-4].strip(". ")
    return (name + ".zip") if name else "Metin2Client.zip"

# The name this server goes by, shown in the header and the browser tab. It is a
# config key so a different install can call itself something else without the
# panel needing to be edited.
BRAND = str(CONF.get("brand", "") or "").strip() or "Singleplayer Official Metin2"

# The community's Discord. DELIBERATELY NOT CONFIGURABLE, and that is the point:
# it is where this project posts what changed and where a player reports a bug,
# so it is part of what the panel IS rather than something each install decides.
# An operator who wants a different address edits this line, which is a change
# to the software and shows up as one -- not a setting that quietly diverges
# between installs and leaves players pointed at nothing.
DISCORD_URL = "https://discord.gg/SSHajSeHm"

CLIENT_NAME  = str(CONF.get("client_name", "Metin2 Client") or "").strip()
CLIENT_FILE  = _client_download_name(CLIENT_NAME)
# shown on the download button; empty means "no name configured" -> t('download')
CLIENT_LABEL = str(CONF.get("client_name", "") or "").strip()

# Optional: hand the download off to somewhere else (MEGA, Google Drive, your own
# https:// site) instead of serving the 1 GB zip from this panel.
#
# Why you may want this: Windows Defender flags downloads that arrive over plain
# http:// from a bare IP address on an unusual port — it reports them as
# "Trojan:Win32/MalUri" ("malicious URI"). That verdict is about the ADDRESS, not
# the file; the very same zip fetched from an https:// link with a real domain
# name is accepted. Serving a gigabyte through Flask's development server is also
# slow, so pointing players at proper hosting fixes both problems at once.
def _clean_client_url(u):
    u = str(u or "").strip()
    # only ever emit a link we would be happy to put in an href
    if u[:8].lower() == "https://" or u[:7].lower() == "http://":
        return u if len(u) < 2000 and "\n" not in u and "\r" not in u else ""
    return ""

CLIENT_URL = _clean_client_url(CONF.get("client_url", ""))

# =============================================================================
#  The client that runs in a browser.
#
#  Three things have to be true before the panel offers it, and it checks all
#  three rather than assuming any of them:
#
#    1. the operator switched it on           M2PANEL_BROWSER_PLAY=1
#    2. a browser client is actually here     browser/current/index.html on the
#                                             volume, or browser/index.html
#    3. the bridge answers                    it is in a compose profile and is
#                                             not started by `up -d'
#
#  With any of them missing the panel shows nothing, serves nothing under /play
#  and contacts nothing. A server that has never heard of this is unaffected.
#
#  The browser client is not in this repository and never will be: it is built
#  from game data that is not ours to redistribute, the same reason client.zip
#  is not here either. It is placed on the panel's data volume by hand.
# =============================================================================
BROWSER_PLAY = bool(CONF.get("browser_play", False))
BROWSER_DIR  = _env_path("M2PANEL_BROWSER_DIR",
                         str(CONF.get("browser_dir", "") or os.path.join(PANEL_DIR, "browser")))

# Where the page is told to dial.
#
# NOT a path on this origin, and that is the client's decision rather than a
# preference: it reads the address out of its own URL with
# /[?&]serverHost=([A-Za-z0-9.\-]+)/, a character class that excludes "/" on
# purpose (mainPosix.cpp says so in as many words), so the bridge can only ever
# be named as host:port. Behind TLS that means port 443 with nginx routing /to/
# and /ping through to it -- the same origin as this page, which is why nothing
# extra has to be opened or certified. Without TLS it is the bridge's own port.
try:
    BRIDGE_PORT = int(CONF.get("bridge_port", 7789))
except (TypeError, ValueError):
    BRIDGE_PORT = 7789
# Where the PANEL asks the bridge what it will connect to. Inside the compose
# network, and nothing to do with what the browser is told.
BRIDGE_HOST = str(CONF.get("bridge_host", "wsbridge") or "wsbridge")

# How much of the game's data the page keeps in memory, in MB, as ?webfsCache=.
#
# This is the setting that decides whether the game stutters. The client's data
# is fetched in 4.2 MB chunks, and a chunk the page does not already hold is
# read with a SYNCHRONOUS request on the main thread -- the frame stops until it
# returns. The browser's own HTTP cache does not save that: the bytes still have
# to be handed back through a binary string and converted, 4.2 MB at a time, in
# the middle of a frame. Only a hit in the page's own memory costs nothing.
#
# The page defaults to 96 MB, which holds 23 of the 420 chunks. 768 holds around
# 180 -- comfortably more than a session in one region touches, so the stutter
# happens once per chunk instead of every time one is evicted. Raise it for
# players with plenty of RAM, lower it for a machine that is short of it; 0
# leaves the page's own default alone.
try:
    BROWSER_CACHE_MB = int(CONF.get("browser_cache_mb", 768))
except (TypeError, ValueError):
    BROWSER_CACHE_MB = 768
if BROWSER_CACHE_MB < 0 or BROWSER_CACHE_MB > 8192:
    BROWSER_CACHE_MB = 768

_BRIDGE_CACHE = {"at": 0.0, "ports": None}
_BRIDGE_LOCK  = threading.Lock()

def browser_root():
    """The directory the browser client actually lives in.

    Two layouts have to work, and which one is present is not a preference:

      browser/index.html          placed by hand, the original arrangement
      browser/current/index.html  installed by fetch-web-client.sh, where
                                  `current' is a symlink to browser/v<engine>-
                                  <data> so that an upgrade is one atomic
                                  rename(2) and a downgrade is the same

    So look for the versioned layout first and fall back to the flat one. The
    check is a stat per call rather than a value worked out at import, because
    the client is routinely installed into a running panel -- deciding this
    once at startup is what made the button stay hidden after an install until
    somebody restarted the container.
    """
    try:
        versioned = os.path.join(BROWSER_DIR, "current")
        if os.path.isfile(os.path.join(versioned, "index.html")):
            return versioned
    except OSError:
        pass
    return BROWSER_DIR

def browser_client_ready():
    """Is there a browser client on the volume to serve?"""
    if not BROWSER_PLAY:
        return False
    try:
        return os.path.isfile(os.path.join(browser_root(), "index.html"))
    except OSError:
        return False

def bridge_ports():
    """The ports the bridge will connect to, or None when it is not running.

    Asked of the bridge itself rather than worked out here: it is the only
    place that knows, and two copies of that arithmetic would eventually
    disagree about a server with more than one channel. It doubles as the
    "is it up" check -- the button is not offered when this is None.
    """
    now = time.time()
    with _BRIDGE_LOCK:
        if now - _BRIDGE_CACHE["at"] < 30:
            return _BRIDGE_CACHE["ports"]
    ports = None
    try:
        url = "http://%s:7789/ports" % BRIDGE_HOST
        with urllib.request.urlopen(url, timeout=3) as r:
            data = json.loads(r.read(8192).decode("utf-8", "replace"))
        raw = data.get("ports") or []
        if isinstance(raw, list):
            ports = [int(p) for p in raw if isinstance(p, int) or str(p).isdigit()]
    except Exception:
        # Not running, or not reachable. Both mean the same thing to the page:
        # there is nothing to connect through, so do not offer to.
        ports = None
    with _BRIDGE_LOCK:
        _BRIDGE_CACHE["at"] = now
        _BRIDGE_CACHE["ports"] = ports
    return ports

def browser_play_ready():
    """All three conditions for offering the button, and it is three on purpose.

    A client on the volume with no bridge running is a button that leads to a
    page which asks the player for a proxy address -- which they cannot know
    and should never be asked for. So the bridge is probed too, and the answer
    is cached for 30 seconds like every other live check on the front page.
    """
    return browser_client_ready() and bridge_ports() is not None

def behind_proxy():
    """Did this request arrive through the reverse proxy?

    Asked of the SETTING as well as of the request, because by the time this
    runs the evidence may be gone. waitress removes the X-Forwarded-* headers
    it does not trust -- clear_untrusted_proxy_headers is on by default -- and
    when the installer has told it to trust the proxy it CONSUMES them instead,
    applying them to the scheme, the host and the client address and then
    taking them out of the environment. Either way the app sees no header.
    That is what made this look like a panel bug for so long: the panel's own
    X-Forwarded handling was correct and never ran, because nothing reached it.

    _LocalProxyFix still covers the case with no container and no trusted
    proxy, where the headers arrive untouched -- hence the two tests.
    """
    return bool(TRUST_PROXY or request.environ.get("panel.via_proxy"))

def bridge_endpoint():
    """(host, port, tls) for THIS request -- what the page must be told.

    Behind the proxy the bridge is reached on the panel's own address and port,
    because nginx routes /to/ and /ping to it: same origin, same certificate,
    nothing extra opened. Without a proxy it is this machine on the bridge's
    own port, and the page is plain HTTP anyway.
    """
    host = request.host.split(":")[0]
    if behind_proxy():
        # The port the visitor actually reached, which is the one their browser
        # will dial again -- not the panel's internal one.
        if ":" in request.host:
            port = int(request.host.rsplit(":", 1)[1])
        else:
            port = 443 if request.is_secure else 80
        return host, port, request.is_secure
    return host, BRIDGE_PORT, False


def play_url():
    """The link behind the button: the page, told where the bridge is.

    ?serverHost / ?serverPort / ?serverTLS are the client's own parameters. With
    them present the page skips its connection dialog entirely and boots
    straight into the game (shell.html), which is what a player arriving from
    this panel should get -- they have already chosen a server by being here.
    """
    host, port, tls = bridge_endpoint()
    url = "/play/?serverHost=%s&serverPort=%d" % (urllib.parse.quote(host, safe=""), port)
    if tls:
        url += "&serverTLS=1"
    if BROWSER_CACHE_MB:
        url += "&webfsCache=%d" % BROWSER_CACHE_MB
    return url

# =============================================================================
#  Which version this is, and whether a newer one has been published.
#
#  Three separate things, deliberately kept separate:
#
#    1. the version      baked into the image at build time; a fact, no network
#    2. the check        one HTTPS request a day to the project's repository,
#                        switchable off, and harmless when it fails
#    3. applying it      off unless the operator switches it on, and even then
#                        this file does not perform the update -- see the
#                        "update request" block further down
#
#  The only one of the three that touches the internet is (2), and it is the
#  only thing in this entire project that contacts anything by itself. That is
#  worth stating plainly rather than burying, so the panel says so on its own
#  patch-log page, in the operator's language, with the setting that turns it
#  off written out.
# =============================================================================

# MAJOR.MINOR.PATCH and nothing else. Anything that does not match this is not
# a version as far as this panel is concerned -- which is what makes a fetched
# VERSION file safe to act on: it is either three numbers or it is ignored.
_SEMVER_RE = re.compile(r"^(\d{1,5})\.(\d{1,5})\.(\d{1,5})$")

def semver(s):
    """(major, minor, patch) or None when this is not a version string."""
    m = _SEMVER_RE.match(str(s or "").strip())
    return tuple(int(g) for g in m.groups()) if m else None

def semver_newer(candidate, current):
    """Is 'candidate' strictly newer than 'current'?

    False whenever either one cannot be read as a version. An unknown current
    version therefore never produces an update notice -- the honest answer to
    "are you behind?" when you do not know what you are is "I cannot say", and
    an unknown version is far more likely to be a development checkout than an
    old release.
    """
    a, b = semver(candidate), semver(current)
    return bool(a and b and a > b)

# VERSION and CHANGELOG.md are staged next to admin_panel.py by
# prepare-context.sh and baked into the panel image. In a source checkout they
# are one level up (files/admin_panel.py, VERSION at the root), which is the
# second place looked at, so a developer running this straight out of the tree
# sees the right number too.
VERSION_FILE   = _env_path("M2PANEL_VERSION_FILE", os.path.join(_HERE, "VERSION"))
CHANGELOG_FILE = _env_path("M2PANEL_CHANGELOG", os.path.join(_HERE, "CHANGELOG.md"))

# How much changelog is ever held or rendered, local or fetched. A changelog
# grows by a few hundred bytes a release, so this is years of them; it is here
# to put a number on "text from the network" rather than to be reached.
CHANGELOG_MAX = 200_000

def _read_text_file(path, limit):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read(limit)
    except OSError:
        return ""

def _read_version():
    env = os.environ.get("M2PANEL_VERSION", "").strip()
    if semver(env):
        return env
    for path in (VERSION_FILE, os.path.join(_HERE, os.pardir, "VERSION")):
        raw = _read_text_file(path, 64).strip().splitlines()
        if raw and semver(raw[0]):
            return raw[0].strip()
    return ""          # unknown, and said as "unknown" -- never invented

PANEL_VERSION = _read_version()

def changelog_split(md, version):
    """Split a changelog at `version`: (newer than it, it and everything older).

    CHANGELOG.md is cumulative, so the published copy contains every release
    including the ones already installed. Showing it whole next to the local
    file printed the same entries twice. Split at the running version and each
    side has something the other does not: what an update would bring, and what
    is already here.

    Sections start at a line like "## 1.2.3 — 2026-08-10". Anything before the
    first of those is the file's own preamble and belongs with the history,
    not with the news. A version that cannot be parsed puts everything in the
    "newer" half, which is the safe way round: better to show a release twice
    than to hide one.
    """
    head, newer, older, cur = [], [], [], None
    for line in (md or "").splitlines(True):
        m = re.match(r'##\s+(\d{1,5}\.\d{1,5}\.\d{1,5})\b', line)
        if m:
            cur = newer if (not version or semver_newer(m.group(1), version)) else older
        (cur if cur is not None else head).append(line)
    return "".join(newer), "".join(head) + "".join(older)

def local_changelog():
    """The changelog for the build that is running, or "" when it is not here."""
    for path in (CHANGELOG_FILE, os.path.join(_HERE, os.pardir, "CHANGELOG.md")):
        text = _read_text_file(path, CHANGELOG_MAX)
        if text.strip():
            return text
    return ""

# ---- the check --------------------------------------------------------------
# Where the published files are. Raw text over HTTPS, from a fixed path: no API,
# no HTML, no redirect chain to somewhere interesting, and nothing that is ever
# executed, unpacked or written to disk as code. Two files are read and only
# two: VERSION (64 bytes, and only three numbers of it are believed) and
# CHANGELOG.md (text, escaped before it is ever shown).
UPDATE_BASE_URL = _env_path(
    "M2PANEL_UPDATE_URL",
    "https://raw.githubusercontent.com/TieruYT/"
    "metin2-playerbots/main")

UPDATE_TIMEOUT  = 8             # seconds, hard, on every network operation
UPDATE_EVERY    = 24 * 3600     # after a successful check
UPDATE_RETRY    = 6 * 3600      # after a failed one -- four tries a day at most
UPDATE_STATE    = _env_path("M2PANEL_UPDATE_STATE", os.path.join(PANEL_DIR, "update.json"))

# On unless the operator turns it off. Off is a supported, first-class state:
# nothing in the panel degrades, no page changes shape, and the patch log still
# shows the changelog of the build that is running.
UPDATE_CHECK = bool(CONF.get("update_check", True)) and UPDATE_BASE_URL.startswith("https://")

# Cached across restarts, on the panel's data volume: a restart is not a reason
# to ask GitHub anything, and on a server that is restarted often it would turn
# "once a day" into "every few minutes".
_UPD = {"checked": 0.0,   # last SUCCESSFUL check
        "tried":   0.0,   # last attempt, successful or not
        "latest":  "",    # last version seen published
        "notes":   "",    # its changelog, fetched only when it is newer
        "error":   ""}    # short, non-technical reason the last attempt failed

_UPD_LOCK = threading.Lock()

def _upd_load():
    try:
        with open(UPDATE_STATE, encoding="utf-8") as f:
            saved = json.load(f)
    except (OSError, ValueError):
        return
    if not isinstance(saved, dict):
        return
    with _UPD_LOCK:
        for key in ("checked", "tried"):
            try:
                _UPD[key] = float(saved.get(key) or 0.0)
            except (TypeError, ValueError):
                pass
        latest = str(saved.get("latest") or "")
        _UPD["latest"] = latest if semver(latest) else ""
        _UPD["notes"]  = str(saved.get("notes") or "")[:CHANGELOG_MAX]
        _UPD["error"]  = str(saved.get("error") or "")[:200]

def _upd_save():
    tmp = UPDATE_STATE + ".new"
    try:
        with _UPD_LOCK:
            payload = dict(_UPD)
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(payload, f)
        os.replace(tmp, UPDATE_STATE)
    except OSError:
        # A read-only or missing data directory costs nothing but the cache:
        # the check still works, it just starts over after a restart.
        try:
            os.unlink(tmp)
        except OSError:
            pass

def _update_fetch(url, limit):
    """One GET, one hard timeout, at most 'limit' bytes, decoded as text.

    Deliberately unclever: no session, no cookies, no authentication, no JSON,
    no HTML. urllib is in the standard library, so this adds no dependency to a
    panel whose dependency list is three packages long.
    """
    req = urllib.request.Request(url, method="GET", headers={
        # Honest about who is calling. GitHub sees this, and so would anyone
        # else the operator points M2PANEL_UPDATE_URL at.
        "User-Agent": "metin2-panel/%s (+%s)" % (PANEL_VERSION or "unknown",
                                                 "https://github.com/TieruYT/"
                                                 "metin2-playerbots"),
        "Accept": "text/plain",
    })
    with urllib.request.urlopen(req, timeout=UPDATE_TIMEOUT) as resp:
        # A redirect away from HTTPS would be a downgrade; refuse it rather
        # than follow it quietly.
        if not str(resp.geturl()).startswith("https://"):
            raise urllib.error.URLError("the answer came from a plain-HTTP address")
        if getattr(resp, "status", 200) != 200:
            raise urllib.error.URLError("HTTP %s" % resp.status)
        return resp.read(limit).decode("utf-8", "replace")

def _update_check_now():
    """One attempt. Never raises: a failure is a recorded fact, not an event."""
    now = time.time()
    try:
        raw = _update_fetch(UPDATE_BASE_URL + "/VERSION", 64)
        first = (raw.strip().splitlines() or [""])[0].strip()
        if not semver(first):
            raise ValueError("the published VERSION is not a version")
        notes = ""
        if semver_newer(first, PANEL_VERSION):
            # Only now is the changelog worth the bytes -- and only then does
            # anything get shown, so there is nothing to fetch otherwise.
            try:
                notes = _update_fetch(UPDATE_BASE_URL + "/CHANGELOG.md", CHANGELOG_MAX)
            except Exception:
                notes = ""      # the version alone is still worth having
        with _UPD_LOCK:
            _UPD.update(checked=now, tried=now, latest=first, notes=notes, error="")
    except Exception as exc:
        with _UPD_LOCK:
            # Keep whatever was known before; only the attempt failed.
            _UPD.update(tried=now, error=str(exc)[:200] or exc.__class__.__name__)
    _upd_save()

def _update_worker():
    """The whole of the panel's outbound traffic, in one background thread.

    Off the request path entirely: no page render ever waits for the network,
    so a machine with no route to the internet renders exactly as fast as one
    with a good one. The thread is a daemon, so it never delays a shutdown.
    """
    # A little jitter, and never during startup: the first page load should not
    # compete with a DNS lookup, and a few hundred servers that were all
    # restarted by the same power cut should not arrive at GitHub together.
    time.sleep(30 + random.random() * 120)
    while True:
        try:
            with _UPD_LOCK:
                tried, ok_at, failed = _UPD["tried"], _UPD["checked"], bool(_UPD["error"])
            due = (tried + UPDATE_RETRY) if failed or not ok_at else (ok_at + UPDATE_EVERY)
            if time.time() >= due:
                _update_check_now()
        except Exception:
            pass            # a bug in here must not take the thread down
        time.sleep(300)

def update_state():
    """What the templates ask. Reads memory only -- never the network."""
    with _UPD_LOCK:
        latest, checked, error = _UPD["latest"], _UPD["checked"], _UPD["error"]
    return {"enabled":   UPDATE_CHECK,
            "current":   PANEL_VERSION,
            "latest":    latest,
            "available": semver_newer(latest, PANEL_VERSION),
            "checked":   checked,
            "error":     error}

def update_notes():
    """The published changelog, when there is a newer version. Text, not HTML."""
    with _UPD_LOCK:
        return _UPD["notes"] if semver_newer(_UPD["latest"], PANEL_VERSION) else ""

_upd_load()
if UPDATE_CHECK:
    threading.Thread(target=_update_worker, name="update-check", daemon=True).start()

# ---- Markdown, the six kinds of it this project's changelog uses -------------
#  The panel depends on flask, PyMySQL and waitress. A Markdown library for
#  headings, bullets, bold and links would be a fourth dependency, with its own
#  release schedule and its own history of HTML-injection bugs, on the one page
#  that renders text fetched over the network. So this renders it here.
#
#  The security property is the ordering, and it is the whole design: every
#  line is HTML-escaped FIRST, and the formatting is then applied to text that
#  can no longer contain a tag, an attribute or a quote. A changelog that
#  contains <script> shows the characters <script>; a link whose target is
#  javascript: or contains a quote is printed as its own words instead of
#  becoming a link. There is no path by which the fetched file can add markup,
#  because by the time anything is added the markup characters are gone.
_MD_LINK = re.compile(r"\[([^\]\[]{1,200})\]\(([^()\s]{1,500})\)")
_MD_BOLD = re.compile(r"\*\*([^*]{1,300})\*\*")
_MD_ITAL = re.compile(r"(?<![*\w])\*([^*\n]{1,300})\*(?![*\w])")
_MD_HEAD = re.compile(r"^(#{1,6})\s+(.*)$")
_MD_BULL = re.compile(r"^(\s*)[-*+]\s+(.*)$")
_MD_NUMB = re.compile(r"^(\s*)\d{1,3}[.)]\s+(.*)$")
_MD_RULE = re.compile(r"^\s*([-*_])(\s*\1){2,}\s*$")

def _md_url_ok(url):
    """Is this escaped URL safe to put inside href="..."?

    It is checked AFTER escaping, so a quote can only be here as an entity and
    could not close the attribute anyway. Both are required: an http(s) scheme
    -- which rules out javascript:, data: and vbscript: -- and no character
    that would end the attribute even if escaping had somehow been skipped.
    """
    if not (url.startswith("https://") or url.startswith("http://")):
        return False
    if len(url) > 500:
        return False
    if any(bad in url for bad in ('"', "'", "<", ">", "&quot;", "&#34;", "&#39;", "&lt;", "&gt;")):
        return False
    return not re.search(r"\s", url)

def _md_inline(chunk):
    """Links, bold and italics inside one already-escaped run of text."""
    def link(m):
        text, url = m.group(1), m.group(2)
        if _md_url_ok(url):
            return '<a href="%s" rel="noopener noreferrer nofollow" target="_blank">%s</a>' % (url, text)
        # A link to a file in the repository (UPDATING.md) rather than to a
        # site: the words are the useful part, and inventing an address for
        # them would send the reader somewhere this panel cannot vouch for.
        return text
    out = _MD_LINK.sub(link, chunk)
    out = _MD_BOLD.sub(r"<strong>\1</strong>", out)
    out = _MD_ITAL.sub(r"<em>\1</em>", out)
    return out

def _md_text(raw_line):
    """One line of Markdown -> one line of safe HTML."""
    parts = str(escape(raw_line)).split("`")
    if len(parts) % 2 == 0:          # an odd number of backticks: the last is stray
        parts = parts[:-2] + ["`".join(parts[-2:])]
    return "".join("<code>%s</code>" % p if i % 2 else _md_inline(p)
                   for i, p in enumerate(parts))

def md_to_html(text):
    """Render the subset of Markdown this project's changelog is written in.

    Headings, horizontal rules, bullet and numbered lists (one level of
    nesting), fenced code blocks, paragraphs, and inline code / bold / italic /
    links. Anything else arrives as its own text, which for a changelog is a
    perfectly good outcome.
    """
    text = str(text or "")[:CHANGELOG_MAX].replace("\r\n", "\n").replace("\r", "\n")
    out, para, lists, fence = [], [], [], None

    def close_para():
        if para:
            out.append("<p>%s</p>" % " ".join(para))
            del para[:]

    def close_lists(depth=0):
        while len(lists) > depth:
            out.append("</%s>" % lists.pop())

    for line in text.split("\n"):
        if fence is not None:
            if line.strip().startswith("```"):
                out.append("<pre><code>%s</code></pre>" % escape("\n".join(fence)))
                fence = None
            else:
                fence.append(line)
            continue
        if line.strip().startswith("```"):
            close_para(); close_lists(); fence = []
            continue
        if not line.strip():
            close_para(); close_lists()
            continue
        if _MD_RULE.match(line):
            close_para(); close_lists()
            out.append("<hr>")
            continue
        m = _MD_HEAD.match(line)
        if m:
            close_para(); close_lists()
            level = min(len(m.group(1)) + 1, 5)      # '#' becomes <h2>: <h1> is the page
            out.append("<h%d>%s</h%d>" % (level, _md_text(m.group(2).rstrip("# ")), level))
            continue
        m = _MD_BULL.match(line) or _MD_NUMB.match(line)
        if m:
            close_para()
            kind = "ul" if _MD_BULL.match(line) else "ol"
            depth = 1 if len(m.group(1)) >= 2 else 0     # one level of nesting, no more
            close_lists(depth + 1)
            while len(lists) <= depth:
                lists.append(kind)
                out.append("<%s>" % kind)
            out.append("<li>%s</li>" % _md_text(m.group(2)))
            continue
        if lists:
            # An indented continuation of the bullet above it.
            out.append(" " + _md_text(line.strip()))
            continue
        para.append(_md_text(line.strip()))

    if fence is not None:                            # unterminated fence
        out.append("<pre><code>%s</code></pre>" % escape("\n".join(fence)))
    close_para(); close_lists()
    # Markup(): every fragment above is either a literal tag written here or a
    # value that went through escape() first. Nothing else reaches this point.
    return Markup("\n".join(out))

# =============================================================================
#  The update request -- what the panel may do, which is very little.
#
#  The panel cannot update this server, and it is important that it cannot.
#  It runs as an unprivileged user in a container, it is the one process here
#  that faces the internet, it takes registrations and serves files, and
#  updating means rebuilding images and recreating containers -- which needs
#  the Docker socket, which is root on the host. A panel with that socket is a
#  panel whose next bug is a host takeover.
#
#  So this is the rates pattern again (see apply_rates.sh and m2-rates): the
#  panel writes a request into a directory shared with a small watcher, and the
#  watcher does the work. The request carries an id and the version the panel
#  believed was published, and the watcher takes ORDERS from neither -- it uses
#  the id to avoid doing the same thing twice, logs the version, and then runs
#  one fixed sequence that is written down in its own source. There is no field
#  in this file that can tell it what to run, where to fetch from, or what to
#  remove.
#
#  Off unless the operator switches it on, in two independent places: this flag
#  (M2_UPDATE_APPLY), and the compose profile that starts the watcher at all.
#  With either one absent, nothing here has an effect and no button is shown.
# =============================================================================
UPDATE_APPLY   = bool(CONF.get("update_apply", False))
UPDATE_SPOOL   = _env_path("M2PANEL_UPDATE_SPOOL", "/opt/m2update")
UPDATE_REQUEST = os.path.join(UPDATE_SPOOL, "request")
UPDATE_STATUS  = os.path.join(UPDATE_SPOOL, "update.status")
UPDATE_LOGFILE = os.path.join(UPDATE_SPOOL, "update.log")
UPDATE_BEAT    = os.path.join(UPDATE_SPOOL, "watcher")
UPDATE_LOG_TAIL = 16_384        # bytes of the watcher's log shown on the page

def updater_ready():
    """Is the watcher actually running on the other side of the spool?

    It touches a file every few seconds. Without that, the button would be
    offered, the request would be written, and nothing would ever happen --
    which looks exactly like a hung update.
    """
    try:
        return (time.time() - os.stat(UPDATE_BEAT).st_mtime) < 120
    except OSError:
        return False

def update_status():
    """The 'key=value' note the watcher leaves behind. Empty when there is none."""
    out = {}
    try:
        with open(UPDATE_STATUS, encoding="utf-8", errors="replace") as f:
            for line in f.read(8192).splitlines():
                k, sep, v = line.partition("=")
                if sep:
                    out[k.strip()] = v.strip()[:400]
    except OSError:
        pass
    return out

def update_log_tail():
    """The last few KB of what the watcher has printed. Text, never HTML."""
    try:
        size = os.path.getsize(UPDATE_LOGFILE)
        with open(UPDATE_LOGFILE, encoding="utf-8", errors="replace") as f:
            if size > UPDATE_LOG_TAIL:
                f.seek(size - UPDATE_LOG_TAIL)
                f.readline()                 # drop the half line seek landed in
            return f.read(UPDATE_LOG_TAIL)
    except OSError:
        return ""

def update_write_status(state):
    """Say 'it is queued' straight away, so the page that opens next is honest."""
    old = os.umask(0o007)
    try:
        with open(UPDATE_STATUS, "w", encoding="utf-8") as f:
            f.write("state=%s\ntime=%d\n" % (state, int(time.time())))
    except OSError:
        pass
    finally:
        os.umask(old)

def update_can_apply():
    """All three conditions, in one place: switched on, watcher up, and behind."""
    return bool(UPDATE_APPLY and updater_ready() and update_state()["available"])

def update_request_write(version):
    """Leave the request in the spool. Returns True when it is safely in place.

    Everything long -- fetching, building, restarting -- happens on the other
    side. This writes about 60 bytes and returns, so the browser gets its
    answer immediately and the progress page takes over from there.
    """
    try:
        os.makedirs(UPDATE_SPOOL, exist_ok=True)
    except OSError:
        return False
    tmp = UPDATE_REQUEST + ".new"
    old = os.umask(0o007)         # the watcher runs as another user, same group
    try:
        with open(tmp, "w", encoding="utf-8") as f:
            f.write("id=%d-%d\nversion=%s\ntime=%d\n"
                    % (int(time.time()), os.getpid(),
                       version if semver(version) else "", int(time.time())))
        os.replace(tmp, UPDATE_REQUEST)
        return True
    except OSError:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        return False
    finally:
        os.umask(old)

# item database (built from item_proto): [{v:vnum, n:name, k:search keywords, c:category}, ...]
try:
    with open(ITEMS_PATH, encoding="utf-8") as f:
        ITEMS = json.load(f)
except Exception:
    ITEMS = []
# vnum -> display name, for the read-only inventory view
ITEM_NAMES = {it["v"]: it["n"] for it in ITEMS}
ITEM_NAMES_PL = {}

# The stock serverfiles already ship a complete Polish locale table.  The
# context-preparation script places it next to this module so the map does not
# have to maintain a second, inevitably incomplete, hand-written item list.
# r40250's Polish table is Windows-1250 encoded.
try:
    _pl_item_names_path = os.path.join(os.path.dirname(__file__), "item_names_pl.txt")
    with open(_pl_item_names_path, "r", encoding="cp1250", errors="replace") as _pl_names:
        for _line in _pl_names:
            _parts = _line.rstrip("\r\n").split("\t", 1)
            if len(_parts) != 2 or not _parts[0].isdigit():
                continue
            ITEM_NAMES_PL[int(_parts[0])] = _parts[1].strip()
except (OSError, UnicodeError):
    ITEM_NAMES_PL = {}

_ITEM_PL_EXACT = {
    "Full Moon Sword": "Miecz Pełni Księżyca",
    "Red Iron Blade": "Czerwone Żelazne Ostrze",
    "Black Leaf Dirk": "Sztylet Czarnego Liścia",
    "Bull's Horn Bow": "Łuk z Rogu Byka",
    "Antique Bell": "Antyczny Dzwon",
    "Autumn Wind Fan": "Wachlarz Jesiennego Wiatru",
    "Battle Shield": "Tarcza Bojowa",
    "Pentagon Shield": "Tarcza Pięciokątna",
    "Horse Medal": "Medal Konny",
    "Red Potion": "Czerwona Mikstura",
    "Blue Potion": "Niebieska Mikstura",
    "Green Potion": "Zielona Mikstura",
    "Purple Potion": "Fioletowa Mikstura",
}
_ITEM_PL_WORDS = (
    ("Skill Book", "Księga Umiejętności"), ("Armour", "Zbroja"),
    ("Armor", "Zbroja"), ("Earrings", "Kolczyki"),
    ("Bracelet", "Bransoleta"), ("Necklace", "Naszyjnik"),
    ("Helmet", "Hełm"), ("Shield", "Tarcza"), ("Boots", "Buty"),
    ("Shoes", "Buty"), ("Sword", "Miecz"), ("Blade", "Ostrze"),
    ("Dagger", "Sztylet"), ("Dirk", "Sztylet"), ("Bow", "Łuk"),
    ("Bell", "Dzwon"), ("Fan", "Wachlarz"), ("Potion", "Mikstura"),
    ("Stone", "Kamień"), ("Book", "Księga"),
)

def translate_item_name_pl(name):
    """Translate stock item families while preserving refinement suffixes."""
    name = str(name or "")
    for source, translated in _ITEM_PL_EXACT.items():
        if name == source or name.startswith(source + "+") or name.startswith(source + " ("):
            return translated + name[len(source):]
    for source, translated in _ITEM_PL_WORDS:
        name = re.sub(r"\b%s\b" % re.escape(source), translated, name,
                      flags=re.IGNORECASE)
    return name

def localized_item_name(vnum, language=None):
    language = language or (lang() if has_request_context() else "en")
    if language == "pl" and int(vnum or 0) in ITEM_NAMES_PL:
        return ITEM_NAMES_PL[int(vnum or 0)]
    name = ITEM_NAMES.get(vnum, "")
    if not name:
        return ("Przedmiot #%d" if language == "pl" else "Item #%d") % int(vnum or 0)
    return translate_item_name_pl(name) if language == "pl" else name

# ---- UI translations -------------------------------------------------------
LANGS = {"pl": "Polski", "en": "English", "de": "Deutsch", "tr": "Türkçe"}
T = {
 "welcome":      {"en":"Welcome!","de":"Willkommen!","tr":"Hoş geldin!"},
 "admin_hint":   {"en":"If you're the admin, enter your passphrase.","de":"Wenn du der Admin bist, gib deine Passphrase ein.","tr":"Yöneticiysen gizli kelimeni yaz."},
 "passphrase":   {"en":"Passphrase","de":"Passphrase","tr":"Gizli kelime"},
 "login":        {"en":"Log in","de":"Einloggen","tr":"Giriş yap"},
 "logout":       {"en":"Log out","de":"Abmelden","tr":"Çıkış"},
 "player_q":     {"en":"Are you a player?","de":"Bist du ein Spieler?","tr":"Oyuncu musun?"},
 # The frame around the two ways into the game, and the heading of the second
 # of them. Both are set in the same voice as play_title, because they sit
 # next to it and a quieter one would read as the lesser option.
 # Said on the card that comes FIRST, because the account is the step before
 # either way in -- and both ways use the same one, which is not obvious when
 # they are presented as a choice.
 "acc_needed":   {"en":"You play with a game account — the same one in the browser and in the download. Making one takes a moment.",
                  "de":"Zum Spielen brauchst du ein Spiel-Konto — dasselbe im Browser wie im Download. Erstellen dauert einen Moment.",
                  "tr":"Oynamak için bir oyun hesabı gerekir — tarayıcıda ve indirmede aynısı. Oluşturmak bir dakika sürer."},
 "ways_t":       {"en":"HOW TO PLAY","de":"SO KOMMST DU INS SPIEL","tr":"OYUNA NASIL GİRERSİN"},
 "dl_now_t":     {"en":"DOWNLOAD IT NOW","de":"JETZT HERUNTERLADEN","tr":"ŞiMDİ İNDİR"},
 "dl_hint":      {"en":"Download it, extract it anywhere you prefer and just run Metin2Distribute.exe — no installation, everything comes pre-configured. The game is fully portable: copy the folder onto a flash drive and play from any machine you like.",
                  "de":"Herunterladen, an einen beliebigen Ort entpacken und einfach Metin2Distribute.exe starten — keine Installation, alles ist vorkonfiguriert. Das Spiel ist komplett portabel: Kopiere den Ordner auf einen USB-Stick und spiele von jedem Rechner, den du magst.",
                  "tr":"İndir, istediğin yere çıkart ve sadece Metin2Distribute.exe'yi çalıştır — kurulum yok, her şey hazır gelir. Oyun tamamen taşınabilir: klasörü bir USB belleğe kopyala, istediğin bilgisayardan oyna."},
 "download":     {"en":"📥 Download the Game","de":"📥 Spiel herunterladen","tr":"📥 Oyunu İndir"},
 "game_account": {"en":"Game account","de":"Spiel-Konto","tr":"Oyun hesabı"},
 "create_acc":   {"en":"📝 Create account","de":"📝 Konto erstellen","tr":"📝 Kayıt ol"},
 "my_acc":       {"en":"👤 My account","de":"👤 Mein Konto","tr":"👤 Hesabım"},
 "players":      {"en":"Players","de":"Spieler","tr":"Oyuncular"},
 "acc_col":      {"en":"Account","de":"Konto","tr":"Hesap"},
 "srv_online":   {"en":"Server online","de":"Server online","tr":"Sunucu çevrimiçi"},
 # --- download steps & facts ---
 "dl_how":       {"en":"How it works","de":"So geht's","tr":"Nasıl çalışır"},
 "dl_st1":       {"en":"Download the zip","de":"Zip herunterladen","tr":"Zip'i indir"},
 "dl_st2":       {"en":"Extract it anywhere you like — a flash drive works too","de":"Irgendwohin entpacken — auch ein USB-Stick geht","tr":"İstediğin yere çıkart — USB bellek de olur"},
 "dl_st3":       {"en":"Run Metin2Distribute.exe and play","de":"Metin2Distribute.exe starten und spielen","tr":"Metin2Distribute.exe'yi çalıştır ve oyna"},
 "dl_sha":       {"en":"With this fingerprint you can verify the download arrived intact — compare it with what your checksum tool says.",
                  "de":"Mit diesem Fingerabdruck kannst du prüfen, ob der Download heil angekommen ist — vergleiche ihn mit dem, was dein Prüfsummen-Tool sagt.",
                  "tr":"Bu parmak iziyle indirmenin sağlam geldiğini doğrulayabilirsin — sağlama aracının söylediğiyle karşılaştır."},
 # --- the client that runs in a browser ---
 "play_title":   {"en":"PLAY IN YOUR BROWSER NOW","de":"JETZT IM BROWSER SPIELEN","tr":"ŞiMDİ TARAYICINDA OYNA"},
 "play_hint":    {"en":"No download and no installation, the game will open as a Browser Tab!",
                  "de":"Kein Download, keine Installation — das Spiel öffnet sich als Browser-Tab!",
                  "tr":"İndirme yok, kurulum yok — oyun bir tarayıcı sekmesinde açılır!"},
 "play_btn":     {"en":"🎮 Play in the browser","de":"🎮 Im Browser spielen","tr":"🎮 Tarayıcıda oyna"},
 "tip_play":     {"en":"Opens the game in a new tab, so this page keeps running. It is the same server as the downloaded game and the same account — a character made in one is there in the other. The downloaded game runs better; this one needs nothing installed.",
                  "de":"Öffnet das Spiel in einem neuen Tab, diese Seite läuft weiter. Es ist derselbe Server wie beim heruntergeladenen Spiel und dasselbe Konto — ein Charakter aus dem einen ist auch im anderen da. Das heruntergeladene Spiel läuft flüssiger; dieses hier braucht keine Installation.",
                  "tr":"Oyunu yeni bir sekmede açar, bu sayfa açık kalır. İndirilen oyunla aynı sunucu ve aynı hesap — birinde yaptığın karakter diğerinde de vardır. İndirilen oyun daha akıcı çalışır; bu ise hiçbir kurulum istemez."},
 # --- registration polish ---
 "reg_title":    {"en":"Create your game account","de":"Spiel-Konto erstellen","tr":"Oyun hesabı oluştur"},
 "reg_hint":     {"en":"This is the account you'll use to log into the game itself.","de":"Mit diesem Konto meldest du dich im Spiel selbst an.","tr":"Oyuna bu hesapla giriş yapacaksın."},
 "reg_ph_user":  {"en":"Username (4-16 letters/numbers)","de":"Benutzername (4-16 Buchstaben/Zahlen)","tr":"Kullanıcı adı (4-16 harf/rakam)"},
 "reg_ph_pw":    {"en":"Password (at least 6 characters)","de":"Passwort (mindestens 6 Zeichen)","tr":"Şifre (en az 6 karakter)"},
 "reg_ph_pw2":   {"en":"Password again","de":"Passwort wiederholen","tr":"Şifre (tekrar)"},
 "reg_ph_social":{"en":"Delete code — pick any 7 digits","de":"Löschcode — beliebige 7 Ziffern","tr":"Silme kodu — 7 rakam seç"},
 "reg_social_hint":{"en":"💡 The delete code is asked by the game when you delete a character. Pick 7 digits you'll remember (e.g. 1234567).",
                  "de":"💡 Den Löschcode fragt das Spiel ab, wenn du einen Charakter löschst. Wähle 7 Ziffern, die du dir merkst (z. B. 1234567).",
                  "tr":"💡 Silme kodunu oyun, karakter silerken sorar. Hatırlayacağın 7 rakam seç (örn. 1234567)."},
 "reg_free":     {"en":"✓ Name is free","de":"✓ Name ist frei","tr":"✓ İsim boş"},
 "reg_taken":    {"en":"✗ Already taken","de":"✗ Schon vergeben","tr":"✗ Zaten alınmış"},
 "reg_pw_match": {"en":"✓ Passwords match","de":"✓ Passwörter stimmen überein","tr":"✓ Şifreler uyuşuyor"},
 "reg_pw_diff":  {"en":"✗ Passwords differ","de":"✗ Passwörter unterscheiden sich","tr":"✗ Şifreler farklı"},
 "reg_done_title":{"en":"Your account is ready!","de":"Dein Konto ist fertig!","tr":"Hesabın hazır!"},
 "reg_done_next":{"en":"From here to the game:","de":"Von hier bis ins Spiel:","tr":"Buradan oyuna:"},
 "reg_done_login":{"en":"Log in with the username and password you just chose","de":"Melde dich mit dem eben gewählten Namen und Passwort an","tr":"Az önce seçtiğin ad ve şifreyle giriş yap"},
 # Shown after registering when this server offers BOTH ways in. The account is
 # the same either way, and saying so is the whole point: without it the two
 # cards read as two different games.
 "reg_both":    {"en":"Two ways in — take either one. It is the same server and the same account.","de":"Zwei Wege ins Spiel — nimm einen davon. Gleicher Server, gleiches Konto.","tr":"Oyuna iki yol — hangisini istersen. Aynı sunucu, aynı hesap."},
 "reg_or":      {"en":"OR","de":"ODER","tr":"VEYA"},
 "reg_dl_t":    {"en":"Download the game","de":"Spiel herunterladen","tr":"Oyunu indir"},
 # --- admin: player search, activity, inventory ---
 "search_players":{"en":"Filter by character or account…","de":"Nach Charakter oder Konto filtern…","tr":"Karakter veya hesaba göre süz…"},
 "tip_active":   {"en":"Was in the game within the last few minutes.","de":"War in den letzten Minuten im Spiel.","tr":"Son birkaç dakika içinde oyundaydı."},
 "inv_title":    {"en":"Inventory","de":"Inventar","tr":"Envanter"},
 "tip_inv":      {"en":"What this character carries, straight from the database — read-only. Handy for checking before you gift something twice.",
                  "de":"Was dieser Charakter bei sich trägt, direkt aus der Datenbank — nur lesend. Praktisch, um nicht doppelt zu schenken.",
                  "tr":"Bu karakterin üzerindekiler, doğrudan veritabanından — salt okunur. Bir şeyi iki kez hediye etmemek için kullanışlı."},
 "inv_empty":    {"en":"Nothing in the inventory yet.","de":"Noch nichts im Inventar.","tr":"Envanterde henüz bir şey yok."},
 "srv_playing":  {"en":"in game right now","de":"gerade im Spiel","tr":"şu an oyunda"},
 "srv_offline":  {"en":"The game server is down at the moment — it usually comes right back. Downloads and account pages keep working.",
                  "de":"Der Spielserver ist gerade aus — meist ist er gleich wieder da. Download und Konto-Seiten funktionieren weiter.",
                  "tr":"Oyun sunucusu şu an kapalı — genellikle hemen geri gelir. İndirme ve hesap sayfaları çalışmaya devam eder."},
 "tip_srv":      {"en":"Checked live against the game itself, at most every 30 seconds: green means the login server and the first channel both answer. The number is characters the game has saved in the last few minutes, so somebody who just logged in may take a moment to appear, and somebody who just left lingers a little.",
                  "de":"Live am Spiel selbst geprüft, höchstens alle 30 Sekunden: Grün heißt, Login-Server und erster Kanal antworten beide. Die Zahl sind Charaktere, die das Spiel in den letzten Minuten gespeichert hat — wer sich gerade erst eingeloggt hat, erscheint also etwas verzögert, und wer eben gegangen ist, bleibt kurz stehen.",
                  "tr":"Doğrudan oyunun kendisinden, en fazla 30 saniyede bir kontrol edilir: yeşil, giriş sunucusunun ve ilk kanalın yanıt verdiği anlamına gelir. Sayı, oyunun son birkaç dakikada kaydettiği karakterlerdir; yeni giren biri biraz gecikmeyle görünür, yeni çıkan biri ise kısa süre sayılmaya devam eder."},
 "tip_acc_col":  {"en":"The game account this character belongs to — the name the player types at the game login. One account can hold several characters.",
                  "de":"Das Spiel-Konto, zu dem dieser Charakter gehört — der Name, den der Spieler beim Spiel-Login eintippt. Ein Konto kann mehrere Charaktere haben.",
                  "tr":"Bu karakterin bağlı olduğu oyun hesabı — oyuncunun oyun girişinde yazdığı ad. Bir hesapta birden çok karakter olabilir."},
 "tap_hint":     {"en":"Tap a player's name to manage them.","de":"Tippe auf einen Spielernamen, um ihn zu verwalten.","tr":"Yönetmek için oyuncunun adına dokun."},
 "character":    {"en":"Character","de":"Charakter","tr":"Karakter"},
 "level":        {"en":"Level","de":"Level","tr":"Seviye"},
 "last_seen":    {"en":"Last seen","de":"Zuletzt online","tr":"Son görülme"},
 "no_chars":     {"en":"No characters yet. They appear here after the first login.","de":"Noch keine Charaktere. Sie erscheinen nach dem ersten Login.","tr":"Henüz karakter yok. İlk girişten sonra görünecek."},
 "back_players": {"en":"← Back to players","de":"← Zurück zu den Spielern","tr":"← Oyunculara dön"},
 "give_item":    {"en":"🎁 Give an item","de":"🎁 Gegenstand geben","tr":"🎁 Eşya ver"},
 "search_item":  {"en":"Type to search item (e.g. sword, potion)…","de":"Zum Suchen tippen (z.B. Schwert, Trank)…","tr":"Aramak için yaz (örn. kılıç, iksir)…"},
 "category":     {"en":"Category","de":"Kategorie","tr":"Kategori"},
 "qty":          {"en":"How many?","de":"Wie viele?","tr":"Kaç adet?"},
 "send_item":    {"en":"🎁 Send item","de":"🎁 Senden","tr":"🎁 Gönder"},
 "give_gold":    {"en":"💰 Give yang","de":"💰 Yang geben","tr":"💰 Yang ver"},
 "amount":       {"en":"Amount (negative takes yang away)","de":"Menge (negativ = abziehen)","tr":"Miktar (eksi = al)"},
 "send_gold":    {"en":"💰 Send yang","de":"💰 Yang senden","tr":"💰 Yang gönder"},
 "set_level":    {"en":"⭐ Set level","de":"⭐ Level setzen","tr":"⭐ Seviye ayarla"},
 "new_level":    {"en":"New level (1-{max})","de":"Neues Level (1-{max})","tr":"Yeni seviye (1-{max})"},
 "level_range":  {"en":"This server's highest level is {max}, so nothing was changed. Raise M2_MAX_LEVEL in .env and restart to go higher — the game itself stops at 120.",
                  "de":"Das höchste Level dieses Servers ist {max}, es wurde nichts geändert. Für mehr M2_MAX_LEVEL in der .env erhöhen und neu starten — das Spiel selbst endet bei 120.",
                  "tr":"Bu sunucunun en yüksek seviyesi {max}, bu yüzden hiçbir şey değiştirilmedi. Daha yükseği için .env dosyasındaki M2_MAX_LEVEL değerini artırıp yeniden başlat — oyunun kendisi 120'de biter."},
 "change_level": {"en":"⭐ Change level","de":"⭐ Level ändern","tr":"⭐ Seviyeyi değiştir"},
 "teleport":     {"en":"🗺️ Teleport","de":"🗺️ Teleportieren","tr":"🗺️ Işınla"},
 "ingame_only":  {"en":"(works while the player is in game)","de":"(nur wenn der Spieler online ist)","tr":"(oyuncu oyundayken çalışır)"},
 "speed":        {"en":"🏃 Running speed","de":"🏃 Laufgeschwindigkeit","tr":"🏃 Koşma hızı"},
 "apply":        {"en":"🏃 Apply","de":"🏃 Anwenden","tr":"🏃 Uygula"},
 "srch_more":    {"en":"⌄ Show more","de":"⌄ Mehr anzeigen","tr":"⌄ Daha fazla göster"},
 "srch_down":    {"en":"The item list could not be loaded. Reload the page; if it keeps happening, your browser may be blocking the request.",
                  "de":"Die Item-Liste konnte nicht geladen werden. Lade die Seite neu; wenn es weiter passiert, blockiert dein Browser die Anfrage womöglich.",
                  "tr":"Eşya listesi yüklenemedi. Sayfayı yenile; devam ederse tarayıcın isteği engelliyor olabilir."},
 # --- the language the game speaks (not the language of this page) ---
 "sect_setup":   {"en":"Server setup","de":"Servereinrichtung","tr":"Sunucu kurulumu"},
 "sect_setup_hint":{"en":"How this server is set up, rather than what happens on it. You will not need these often, and both of them affect everyone.",
                  "de":"Wie dieser Server eingerichtet ist — nicht, was auf ihm passiert. Beides brauchst du selten, und beides betrifft alle.",
                  "tr":"Bu sunucunun nasıl kurulduğu — üzerinde ne olduğu değil. İkisine de nadiren ihtiyacın olur ve ikisi de herkesi etkiler."},
 "lang_title":   {"en":"🌍 Game language","de":"🌍 Spielsprache","tr":"🌍 Oyun dili"},
 "lang_now":     {"en":"The game is in {lang}.","de":"Das Spiel läuft auf {lang}.","tr":"Oyun {lang} dilinde."},
 "lang_intro":   {"en":"This is the language the server speaks: quest text, system messages, and the names of items and monsters. It is not the language of this page — that is the switcher at the top right. Changing it restarts the game for well under a minute, so anyone playing is briefly disconnected.",
                  "de":"Das ist die Sprache, die der Server spricht: Questtexte, Systemmeldungen und die Namen von Gegenständen und Monstern. Es ist nicht die Sprache dieser Seite — die stellst du oben rechts um. Beim Wechsel startet das Spiel für deutlich unter einer Minute neu, Spieler fliegen also kurz raus.",
                  "tr":"Bu, sunucunun konuştuğu dildir: görev metinleri, sistem mesajları ve eşya ile canavar adları. Bu sayfanın dili değildir — onu sağ üstten değiştirirsin. Değiştirmek oyunu bir dakikadan çok kısa süre yeniden başlatır, oyuncular kısa süre düşer."},
 "lang_apply":   {"en":"🌍 Change the game language","de":"🌍 Spielsprache ändern","tr":"🌍 Oyun dilini değiştir"},
 "lang_asked":   {"en":"Switching the game to {lang}. The server restarts itself in a moment; reload this page to see it done.",
                  "de":"Das Spiel wird auf {lang} umgestellt. Der Server startet gleich neu; lade diese Seite neu, um es bestätigt zu sehen.",
                  "tr":"Oyun {lang} diline geçiriliyor. Sunucu birazdan yeniden başlıyor; tamamlandığını görmek için bu sayfayı yenile."},
 "lang_same":    {"en":"The game is already in {lang}, so nothing was changed.",
                  "de":"Das Spiel läuft bereits auf {lang}, es wurde nichts geändert.",
                  "tr":"Oyun zaten {lang} dilinde, hiçbir şey değiştirilmedi."},
 "lang_nospool": {"en":"The game server could not be reached, so the language was not changed.",
                  "de":"Der Spielserver war nicht erreichbar, die Sprache wurde nicht geändert.",
                  "tr":"Oyun sunucusuna ulaşılamadı, dil değiştirilmedi."},
 "lang_busy":    {"en":"The language is being changed. The game restarts as part of it.",
                  "de":"Die Sprache wird gerade umgestellt. Das Spiel startet dabei neu.",
                  "tr":"Dil değiştiriliyor. Oyun bu sırada yeniden başlıyor."},
 "lang_failed":  {"en":"The language could not be changed, so it is unchanged.",
                  "de":"Die Sprache konnte nicht geändert werden, sie bleibt also unverändert.",
                  "tr":"Dil değiştirilemedi, bu yüzden değişmedi."},
 "lang_unsup":   {"en":"These server files do not carry that language in full, so nothing was changed.",
                  "de":"Diese Serverdateien enthalten diese Sprache nicht vollständig, es wurde nichts geändert.",
                  "tr":"Bu sunucu dosyaları o dili eksiksiz içermiyor, hiçbir şey değiştirilmedi."},
 # --- what a player has to do to their own copy of the game ---
 "lang_cl_t":    {"en":"Players who already downloaded the game","de":"Spieler, die das Spiel schon heruntergeladen haben","tr":"Oyunu zaten indirmiş oyuncular"},
 "lang_cl":      {"en":"The download on this page is built to match, so anybody who gets the game from now on is already in {lang}. Somebody who downloaded it earlier changes their own copy in two steps, in the folder the game is in:",
                  "de":"Der Download auf dieser Seite wird passend gebaut — wer das Spiel ab jetzt holt, hat bereits {lang}. Wer es früher heruntergeladen hat, stellt seine eigene Kopie in zwei Schritten um, im Ordner des Spiels:",
                  "tr":"Bu sayfadaki indirme buna uygun hazırlanır; oyunu bundan sonra alan herkes zaten {lang} dilinde olur. Daha önce indirmiş olan kendi kopyasını, oyunun bulunduğu klasörde iki adımda değiştirir:"},
 "lang_cl_1":    {"en":"Rename locale.cfg to locale_old.cfg","de":"locale.cfg in locale_old.cfg umbenennen","tr":"locale.cfg dosyasını locale_old.cfg olarak yeniden adlandır"},
 "lang_cl_2":    {"en":"Rename {file} to locale.cfg","de":"{file} in locale.cfg umbenennen","tr":"{file} dosyasını locale.cfg olarak yeniden adlandır"},
 "lang_cl_3":    {"en":"Every language is already in the folder, so nothing has to be downloaded again. The game has to be closed while you do it.",
                  "de":"Alle Sprachen liegen bereits im Ordner, es muss also nichts erneut heruntergeladen werden. Das Spiel muss dabei geschlossen sein.",
                  "tr":"Bütün diller zaten klasörde, bu yüzden hiçbir şeyi yeniden indirmek gerekmez. Bunu yaparken oyun kapalı olmalı."},
 "dl_lang":      {"en":"The game is in {lang}.","de":"Das Spiel ist auf {lang}.","tr":"Oyun {lang} dilinde."},
 # Deliberately NOT "tip_lang": that one belongs to the switcher at the top of
 # the page, which changes this panel and nothing else. Two settings with almost
 # the same name is exactly how somebody ends up restarting a server they only
 # meant to read in German.
 "tip_gamelang": {"en":"Changes the language the server speaks — quest text, system messages, item and monster names. The game restarts briefly.",
                  "de":"Ändert die Sprache, die der Server spricht — Questtexte, Systemmeldungen, Item- und Monsternamen. Das Spiel startet dabei kurz neu.",
                  "tr":"Sunucunun konuştuğu dili değiştirir — görev metinleri, sistem mesajları, eşya ve canavar adları. Oyun kısa süre yeniden başlar."},
 "gm_title":     {"en":"🛡️ Game master","de":"🛡️ Spielleiter","tr":"🛡️ Oyun yöneticisi"},
 "gm_apply":     {"en":"🛡️ Set rank","de":"🛡️ Rang setzen","tr":"🛡️ Rütbeyi ayarla"},
 "gm_none":      {"en":"Normal player (no rank)","de":"Normaler Spieler (kein Rang)","tr":"Normal oyuncu (rütbe yok)"},
 "gm_now":       {"en":"Currently: {rank}","de":"Aktuell: {rank}","tr":"Şu an: {rank}"},
 "gm_granted":   {"en":"{name} is now {rank}. It applies right away, in game.",
                  "de":"{name} ist jetzt {rank}. Das gilt sofort, im laufenden Spiel.",
                  "tr":"{name} artık {rank}. Bu, oyun içinde hemen geçerli olur."},
 "gm_removed":   {"en":"{name} is a normal player again. They keep the commands until they log out and back in.",
                  "de":"{name} ist wieder normaler Spieler. Die Befehle bleiben ihm, bis er sich aus- und wieder einloggt.",
                  "tr":"{name} yeniden normal oyuncu. Çıkıp tekrar girene kadar komutlar onda kalır."},
 "gm_noreload":  {"en":"Saved, but the game server could not be told. It will take effect the next time the server starts.",
                  "de":"Gespeichert, aber der Spielserver konnte nicht benachrichtigt werden. Es wirkt beim nächsten Serverstart.",
                  "tr":"Kaydedildi, ancak oyun sunucusuna bildirilemedi. Sunucu bir sonraki başlangıçta geçerli olacak."},
 "cat_all":      {"en":"All","de":"Alle","tr":"Hepsi"},
 "cat_weapon":   {"en":"Weapons","de":"Waffen","tr":"Silahlar"},
 "cat_armor":    {"en":"Armor","de":"Rüstung","tr":"Zırhlar"},
 "cat_usable":   {"en":"Potions/Usable","de":"Tränke/Nutzbar","tr":"İksir/Kullanılabilir"},
 "cat_ds":       {"en":"Dragon Soul","de":"Drachenseele","tr":"Ejder Ruhu"},
 "cat_metin":    {"en":"Metin Stones","de":"Metinsteine","tr":"Metin Taşları"},
 "cat_special":  {"en":"Special","de":"Spezial","tr":"Özel"},
 "cat_other":    {"en":"Other","de":"Sonstige","tr":"Diğer"},
 # --- status & error messages ---
 "db_down":      {"en":"The game database can't be reached right now. The server may be down — please try again in a bit. 🙏",
                  "de":"Die Spiel-Datenbank ist gerade nicht erreichbar. Der Server ist vielleicht aus — bitte versuche es gleich noch einmal. 🙏",
                  "tr":"Oyun veritabanına şu anda ulaşılamıyor. Sunucu kapalı olabilir — birazdan tekrar dene. 🙏"},
 "csrf_bad":     {"en":"For your safety that request was blocked — it didn't come from this page. Please open the panel again and retry. 🔒",
                  "de":"Zu deiner Sicherheit wurde diese Anfrage blockiert — sie kam nicht von dieser Seite. Bitte öffne das Panel neu und versuche es erneut. 🔒",
                  "tr":"Güvenliğin için bu istek engellendi — bu sayfadan gelmedi. Lütfen paneli yeniden aç ve tekrar dene. 🔒"},
 # {max} is MAX_ITEM_COUNT, which depends on the server files (see there):
 # 65,535 on most of them, 255 on the [40250] reference files.
 "qty_range":    {"en":"Please enter a quantity between 1 and {max} — the game can't store more than that in one stack. 🙂",
                  "de":"Bitte gib eine Menge zwischen 1 und {max} ein — mehr passt im Spiel nicht in einen Stapel. 🙂",
                  "tr":"Lütfen 1 ile {max} arasında bir adet gir — oyun tek yığında bundan fazlasını saklayamaz. 🙂"},
 # {conf} is the config file this panel actually read — it is not always
 # /usr/local/etc/m2panel.conf any more (see M2PANEL_CONF).
 "inv_full":     {"en":"The inventory is full — ask the player to free up some space. 🎒 (If their server has more inventory pages, raise \"inventory_slots\" in {conf}.)",
                  "de":"Das Inventar ist voll — bitte den Spieler, Platz zu schaffen. 🎒 (Hat der Server mehr Inventarseiten, erhöhe \"inventory_slots\" in {conf}.)",
                  "tr":"Envanter dolu — oyuncudan yer açmasını iste. 🎒 (Sunucunda daha fazla envanter sayfası varsa {conf} içindeki \"inventory_slots\" değerini artır.)"},
 "ingame_offline":{"en":"🙂 {name} isn't in game right now, and this action only works while they're online. Nothing was changed — ask them to log in and try again.",
                  "de":"🙂 {name} ist gerade nicht im Spiel, und diese Aktion funktioniert nur online. Es wurde nichts geändert — bitte einloggen lassen und erneut versuchen.",
                  "tr":"🙂 {name} şu anda oyunda değil ve bu işlem yalnızca oyundayken çalışır. Hiçbir şey değiştirilmedi — giriş yapmasını isteyip tekrar dene."},
 "ingame_timeout":{"en":"⏳ The in-game helper didn't answer, so this action couldn't be delivered to {name}. Nothing was changed — check that the game server is running and try again in a moment.",
                  "de":"⏳ Der Ingame-Helfer hat nicht geantwortet, deshalb konnte die Aktion {name} nicht zugestellt werden. Es wurde nichts geändert — prüfe, ob der Spielserver läuft, und versuche es gleich noch einmal.",
                  "tr":"⏳ Oyun içi yardımcı yanıt vermedi, bu yüzden işlem {name} oyuncusuna iletilemedi. Hiçbir şey değiştirilmedi — oyun sunucusunun çalıştığını kontrol et ve az sonra tekrar dene."},
 "act_done":     {"en":"✅ Done! The action was applied instantly for {name}.",
                  "de":"✅ Fertig! Die Aktion wurde sofort für {name} angewendet.",
                  "tr":"✅ Tamam! İşlem {name} için anında uygulandı."},
 # Used when nothing is listening in the game, rather than when the player is
 # away -- saying "they weren't in game" to somebody who is standing in it
 # sends them looking for a problem with their character.
 "act_nohelper": {"en":"✅ Written to {name}'s account — it appears the next time they log in.",
                  "de":"✅ Auf das Konto von {name} geschrieben — es erscheint beim nächsten Einloggen.",
                  "tr":"✅ {name} adlı oyuncunun hesabına yazıldı — bir sonraki girişte görünür."},
 "ingame_nohelper":{"en":"⛔ Nothing in the game answered, so {name} was not moved and nothing was changed. Teleport and running speed only work on a character who is logged in, on a server whose in-game helper is running.",
                  "de":"⛔ Aus dem Spiel kam keine Antwort, {name} wurde also nicht bewegt und nichts geändert. Teleportieren und Laufgeschwindigkeit wirken nur auf einen eingeloggten Charakter, auf einem Server, dessen In-Game-Helfer läuft.",
                  "tr":"⛔ Oyundan yanıt gelmedi, bu yüzden {name} taşınmadı ve hiçbir şey değişmedi. Işınlama ve koşma hızı yalnızca giriş yapmış bir karakterde ve oyun içi yardımcısı çalışan bir sunucuda işe yarar."},
 "act_offline":  {"en":"✅ {name} wasn't in game right now — it was applied to their account and will be ready when they log in! 🎉",
                  "de":"✅ {name} war gerade nicht im Spiel — es wurde direkt auf dem Konto angewendet und ist beim nächsten Login da! 🎉",
                  "tr":"✅ {name} şu anda oyunda değildi — işlem hesabına uygulandı, giriş yaptığında hazır olacak! 🎉"},
 "act_late_done":{"en":"✅ Just in time! {name} picked it up in game while we were waiting, so it was applied there — not twice. 🎉",
                  "de":"✅ Gerade noch rechtzeitig! {name} hat es im Spiel erhalten, während wir gewartet haben — es wurde also nur einmal angewendet. 🎉",
                  "tr":"✅ Tam zamanında! {name} beklerken bunu oyun içinde aldı, yani işlem iki kez değil bir kez uygulandı. 🎉"},
 "act_late_other":{"en":"ℹ️ The in-game helper took this over while we were waiting (status: {status}), so nothing was applied twice. Please check {name} and try again if needed.",
                  "de":"ℹ️ Der Ingame-Helfer hat das während des Wartens übernommen (Status: {status}) — es wurde also nichts doppelt angewendet. Bitte prüfe {name} und versuche es bei Bedarf erneut.",
                  "tr":"ℹ️ Oyun içi yardımcı bunu beklerken devraldı (durum: {status}), bu yüzden hiçbir şey iki kez uygulanmadı. Lütfen {name} oyuncusunu kontrol et ve gerekirse tekrar dene."},
 "act_error":    {"en":"Something went wrong ({status}), but no worries — you can just try again.",
                  "de":"Etwas ist schiefgelaufen ({status}), aber keine Sorge — versuche es einfach noch einmal.",
                  "tr":"Bir şeyler ters gitti ({status}) ama merak etme — tekrar deneyebilirsin."},
 "act_novalue":  {"en":"Looks like you forgot to enter a value — could you try again? 🙂",
                  "de":"Da fehlt wohl noch ein Wert — magst du es noch einmal versuchen? 🙂",
                  "tr":"Görünüşe göre bir değer girmeyi unuttun — tekrar dener misin? 🙂"},
 "act_unexpected":{"en":"An unexpected problem occurred. Try again; if it keeps happening, tell the person who set up the server. 🙏",
                  "de":"Es gab ein unerwartetes Problem. Versuche es erneut; wenn es bleibt, sag der Person Bescheid, die den Server eingerichtet hat. 🙏",
                  "tr":"Beklenmeyen bir sorun oluştu. Tekrar dene; devam ederse sunucuyu kuran kişiye haber ver. 🙏"},
 "not_found":    {"en":"Player not found.","de":"Spieler nicht gefunden.","tr":"Oyuncu bulunamadı."},
 # --- server rates ---
   # --- discord ---
   "dc_foot":      {"en":"Discord — news, updates and help",
                    "de":"Discord — Neuigkeiten, Updates und Hilfe",
                    "tr":"Discord — haberler, güncellemeler ve yardım"},
   "dc_title":     {"en":"💬 Come to the Discord",
                    "de":"💬 Komm auf den Discord",
                    "tr":"💬 Discord'a gel"},
   "dc_body":      {"en":"Everything current is there first: what changed in the last update, when the server is down for maintenance, and the people who can answer a question faster than you can search for it. Found a bug? Report it there — it is the quickest way to get it fixed.",
                    "de":"Alles Aktuelle steht dort zuerst: was sich mit dem letzten Update geändert hat, wann der Server für Wartungen weg ist, und die Leute, die eine Frage schneller beantworten, als du sie suchen kannst. Fehler gefunden? Melde ihn dort — das ist der schnellste Weg, ihn loszuwerden.",
                    "tr":"Güncel olan her şey önce orada: son güncellemede ne değişti, sunucu bakım için ne zaman kapalı olacak ve bir soruyu aramandan daha hızlı yanıtlayacak insanlar. Hata mı buldun? Oraya bildir — düzeltilmesinin en hızlı yolu."},
   "dc_btn":       {"en":"💬 Join the Discord",
                    "de":"💬 Discord beitreten",
                    "tr":"💬 Discord'a katıl"},
 "rates_nav":    {"en":"⚙️ Server rates","de":"⚙️ Server-Raten","tr":"⚙️ Sunucu oranları"},
 "rates_open":   {"en":"⚙️ Open server rates","de":"⚙️ Server-Raten öffnen","tr":"⚙️ Sunucu oranlarını aç"},
 "rates_dash_hint":{"en":"Make the whole server give more experience, more items and more yang — handy if you would rather do quests than grind.",
                  "de":"Lass den ganzen Server mehr Erfahrung, mehr Gegenstände und mehr Yang geben — praktisch, wenn du lieber Quests machst als zu grinden.",
                  "tr":"Tüm sunucunun daha çok tecrübe, daha çok eşya ve daha çok yang vermesini sağla — grind yerine görev yapmayı seviyorsan çok işine yarar."},
 "rates_intro":  {"en":"These three numbers decide how quickly the whole server moves. 100% is exactly how the game was made — higher means faster. Saving restarts the game server, so anyone playing gets dropped for a moment.",
                  "de":"Diese drei Zahlen bestimmen, wie schnell der ganze Server läuft. 100% ist genau so, wie das Spiel gemacht wurde — höher heißt schneller. Beim Speichern wird der Spielserver neu gestartet, wer gerade spielt fliegt also kurz raus.",
                  "tr":"Bu üç sayı tüm sunucunun ne kadar hızlı ilerlediğini belirler. 100%, oyunun yapıldığı hâlidir — yüksek olması daha hızlı demektir. Kaydetmek oyun sunucusunu yeniden başlatır, o sırada oynayan varsa kısa bir süre düşer."},
 "rates_exp":    {"en":"Experience","de":"Erfahrung","tr":"Tecrübe"},
 "rates_exp_help":{"en":"How much experience monsters give. 300% means levelling up goes three times as fast.",
                  "de":"Wie viel Erfahrung Monster geben. 300% heißt, du levelst dreimal so schnell.",
                  "tr":"Canavarların verdiği tecrübe. 300% seviye atlamanın üç kat hızlanması demek."},
 "rates_drop":   {"en":"Item drop","de":"Gegenstände","tr":"Eşya düşme"},
 "rates_drop_help":{"en":"How often monsters drop items. 200% means twice as many drops. A chance can never go above certain, so items that always dropped simply keep dropping.",
                  "de":"Wie oft Monster Gegenstände fallen lassen. 200% heißt doppelt so viele. Mehr als sicher geht nicht — was immer gedroppt ist, droppt einfach weiter.",
                  "tr":"Canavarların ne sıklıkla eşya düşürdüğü. 200% iki katı düşme demek. Bir ihtimal kesinliğin üstüne çıkamaz, yani zaten hep düşen eşyalar düşmeye devam eder."},
 "rates_yang":   {"en":"Yang","de":"Yang","tr":"Yang"},
 "rates_yang_help":{"en":"How much money monsters drop when you kill them.",
                  "de":"Wie viel Geld Monster fallen lassen, wenn du sie besiegst.",
                  "tr":"Canavarları öldürdüğünde düşen para miktarı."},
 "rates_percent":{"en":"Percent — 100 is normal","de":"Prozent — 100 ist normal","tr":"Yüzde — 100 normaldir"},
 "rates_current":{"en":"Active right now","de":"Gerade aktiv","tr":"Şu anda geçerli"},
 "rates_presets":{"en":"Or take one of these","de":"Oder nimm eine davon","tr":"Ya da bunlardan birini seç"},
 "rates_presets_hint":{"en":"One tap fills the three boxes in — you still have to save.",
                  "de":"Ein Tipp füllt die drei Felder aus — speichern musst du trotzdem noch.",
                  "tr":"Bir dokunuş üç kutuyu doldurur — yine de kaydetmen gerekir."},
 "rates_p_normal":{"en":"🎯 Normal — exactly like the original game",
                  "de":"🎯 Normal — genau wie im Originalspiel",
                  "tr":"🎯 Normal — orijinal oyundaki gibi"},
 "rates_p_relaxed":{"en":"🌿 Relaxed questing — experience 300%, items 200%, yang 200%",
                  "de":"🌿 Entspannt questen — Erfahrung 300%, Gegenstände 200%, Yang 200%",
                  "tr":"🌿 Rahat görev — tecrübe 300%, eşya 200%, yang 200%"},
 "rates_p_fast": {"en":"🚀 Fast — experience 1000%, items 500%, yang 500%",
                  "de":"🚀 Schnell — Erfahrung 1000%, Gegenstände 500%, Yang 500%",
                  "tr":"🚀 Hızlı — tecrübe 1000%, eşya 500%, yang 500%"},
 "rates_save":   {"en":"💾 Save and restart the server","de":"💾 Speichern und Server neu starten","tr":"💾 Kaydet ve sunucuyu yeniden başlat"},
 "rates_range":  {"en":"Each of the three has to be a whole number between 1 and 10000. Nothing was changed. 🙂",
                  "de":"Alle drei müssen ganze Zahlen zwischen 1 und 10000 sein. Es wurde nichts geändert. 🙂",
                  "tr":"Üçü de 1 ile 10000 arasında tam sayı olmalı. Hiçbir şey değiştirilmedi. 🙂"},
 "rates_saved":  {"en":"✅ Saved! The game server is restarting now and should be back in under a minute. Give this page a reload in a moment to see how it went.",
                  "de":"✅ Gespeichert! Der Spielserver startet gerade neu und sollte in weniger als einer Minute wieder da sein. Lade diese Seite gleich neu, um das Ergebnis zu sehen.",
                  "tr":"✅ Kaydedildi! Oyun sunucusu şimdi yeniden başlıyor, bir dakikadan kısa sürede geri gelmeli. Sonucu görmek için birazdan bu sayfayı yenile."},
 "rates_no_script":{"en":"This server was set up before the rates feature existed, so the little helper that applies them is missing. Run the installer again on the server — it adds the helper and changes nothing else. 🙂",
                  "de":"Dieser Server wurde eingerichtet, bevor es die Raten gab, deshalb fehlt das kleine Hilfsprogramm dafür. Führe das Installationsprogramm auf dem Server noch einmal aus — es ergänzt nur dieses Hilfsprogramm. 🙂",
                  "tr":"Bu sunucu oran özelliği eklenmeden önce kurulmuş, bu yüzden oranları uygulayan küçük yardımcı yok. Sunucuda kurulumu tekrar çalıştır — sadece bu yardımcıyı ekler, başka bir şeye dokunmaz. 🙂"},
 "rates_no_table":{"en":"The rates could not be saved, because the table they live in is not there yet. Running the installer again on the server creates it. 🙏",
                  "de":"Die Raten konnten nicht gespeichert werden, weil die zugehörige Tabelle noch fehlt. Ein erneuter Lauf des Installationsprogramms legt sie an. 🙏",
                  "tr":"Oranlar kaydedilemedi, çünkü bulundukları tablo henüz yok. Sunucuda kurulumu tekrar çalıştırmak bu tabloyu oluşturur. 🙏"},
 "rates_st":     {"en":"How the last change went","de":"Wie die letzte Änderung lief","tr":"Son değişiklik nasıl gitti"},
 "rates_st_running":{"en":"⏳ The rates are being applied and the server is restarting. This normally takes well under a minute.",
                  "de":"⏳ Die Raten werden angewendet und der Server startet neu. Das dauert normalerweise deutlich unter einer Minute.",
                  "tr":"⏳ Oranlar uygulanıyor ve sunucu yeniden başlıyor. Bu genelde bir dakikadan çok kısa sürer."},
 "rates_st_ok":  {"en":"✅ These rates are live on the server.",
                  "de":"✅ Diese Raten sind auf dem Server aktiv.",
                  "tr":"✅ Bu oranlar sunucuda geçerli."},
 "rates_st_unsupported":{"en":"⚠️ These server files cannot have their rates changed, so nothing was applied — whatever you type here will have no effect in the game. This is not something you did wrong; the set of server files simply does not support it.",
                  "de":"⚠️ Bei diesen Serverdateien lassen sich die Raten nicht ändern, es wurde also nichts angewendet — was du hier einträgst, wirkt sich im Spiel nicht aus. Du hast nichts falsch gemacht, diese Serverdateien können es einfach nicht.",
                  "tr":"⚠️ Bu sunucu dosyalarının oranları değiştirilemiyor, bu yüzden hiçbir şey uygulanmadı — buraya ne yazarsan yaz oyunda bir etkisi olmaz. Senin hatan değil; bu sunucu dosyaları bunu desteklemiyor."},
 "rates_st_failed":{"en":"⚠️ Something went wrong while applying the rates, so they may not all be live. The details are in /var/log/m2rates.log on the server.",
                  "de":"⚠️ Beim Anwenden der Raten ist etwas schiefgelaufen, vielleicht sind nicht alle aktiv. Die Einzelheiten stehen auf dem Server in /var/log/m2rates.log.",
                  "tr":"⚠️ Oranlar uygulanırken bir şeyler ters gitti, hepsi geçerli olmayabilir. Ayrıntılar sunucudaki /var/log/m2rates.log dosyasında."},
 "rates_st_no_restart":{"en":"ℹ️ The new rates are saved, but the game server could not be restarted on its own. Restart it yourself and they take effect right away.",
                  "de":"ℹ️ Die neuen Raten sind gespeichert, aber der Spielserver konnte nicht selbst neu gestartet werden. Starte ihn von Hand neu, dann gelten sie sofort.",
                  "tr":"ℹ️ Yeni oranlar kaydedildi ama oyun sunucusu kendi kendine yeniden başlatılamadı. Sen yeniden başlatınca hemen geçerli olurlar."},

 # --- what this project is (shown to everyone, no login needed) ---
 "about_title":  {"en":"What is this?","de":"Was ist das hier?","tr":"Bu nedir?"},
 "about_goal":   {"en":"This is Metin2 as it was in 2014 — the original game, unchanged — running on a server somebody set up for themselves. No item shop, nothing to buy, and none of the grind that only exists to sell you something. You play at your own pace.",
                  "de":"Das hier ist Metin2, wie es 2014 war — das Originalspiel, unverändert — auf einem Server, den sich jemand selbst eingerichtet hat. Kein Item-Shop, nichts zu kaufen, und nichts von dem Grind, den es nur gibt, um dir etwas zu verkaufen. Du spielst in deinem eigenen Tempo.",
                  "tr":"Burada 2014'teki hâliyle Metin2 var — orijinal oyun, değiştirilmemiş — birinin kendisi için kurduğu bir sunucuda çalışıyor. Item shop yok, satın alınacak bir şey yok ve sadece sana bir şey satmak için var olan o grind yok. Kendi hızında oynarsın."},
 "about_hobby":  {"en":"This is a hobby project. Nobody earns anything from it, there is nothing to buy, and there never will be.",
                  "de":"Das hier ist ein Hobbyprojekt. Niemand verdient daran etwas, es gibt nichts zu kaufen, und das wird auch so bleiben.",
                  "tr":"Burası bir hobi projesi. Kimse bundan para kazanmıyor, satın alınacak bir şey yok ve olmayacak da."},
 # Only shown when this server really has a browser client -- a server that
 # offers the download alone must not promise a link that is not there.
 "about_web":    {"en":"You can play straight in your browser — one click, nothing to download and nothing to install. If you would rather have the proper client, download it instead: same server, same account, and a character made in one is there in the other.",
                  "de":"Spielen kannst du direkt im Browser — ein Klick, kein Download, keine Installation. Wer lieber den richtigen Client möchte, lädt ihn herunter: derselbe Server, dasselbe Konto, und ein Charakter aus dem einen ist auch im anderen da.",
                  "tr":"Doğrudan tarayıcında oynayabilirsin — tek tık, indirme yok, kurulum yok. Gerçek istemciyi tercih edersen onu indir: aynı sunucu, aynı hesap; birinde yaptığın karakter diğerinde de vardır."},
 "about_uptime": {"en":"Characters live in this server's own database and nowhere else — nobody has a second copy, and nothing is sent anywhere. Which also means how long this world lasts is entirely up to whoever runs it.",
                  "de":"Charaktere liegen in der Datenbank dieses Servers und sonst nirgends — niemand hat eine zweite Kopie, und es wird nichts irgendwohin übertragen. Das heißt aber auch: Wie lange es diese Welt gibt, entscheidet allein, wer den Server betreibt.",
                  "tr":"Karakterler yalnızca bu sunucunun kendi veritabanında durur — kimsede ikinci bir kopya yok ve hiçbir yere bir şey gönderilmiyor. Bu aynı zamanda şu demek: bu dünyanın ne kadar süreceğine yalnızca sunucuyu işleten kişi karar verir."},
 "about_oss":    {"en":"Everything needed to run this is open source. Anyone can set up exactly the same server on their own machine with a single command — no Metin2 knowledge required.",
                  "de":"Alles, was dafür nötig ist, ist Open Source. Jeder kann sich mit einem einzigen Befehl genau denselben Server selbst aufsetzen — ganz ohne Metin2-Vorwissen.",
                  "tr":"Bunu çalıştırmak için gereken her şey açık kaynak. İsteyen herkes tek bir komutla aynı sunucuyu kendi makinesinde kurabilir — Metin2 bilgisi gerekmez."},
 "about_contact":{"en":"If you feel the server needs adjusting — the rates, movement speed, or anything else — write to",
                  "de":"Wenn du findest, dass am Server etwas angepasst werden sollte — die Raten, die Laufgeschwindigkeit oder irgendetwas anderes — schreib an",
                  "tr":"Sunucuda bir şeyin ayarlanması gerektiğini düşünüyorsan — oranlar, hareket hızı ya da başka herhangi bir şey — şu adrese yaz:"},
 # --- local install: the game is on this machine, there is nothing to fetch ---
 # --- first-run prompt: no account exists on this server yet ---
 "ob_title":     {"en":"Almost ready to play","de":"Fast startklar","tr":"Oynamaya neredeyse hazır"},
 "ob_none":      {"en":"There is no account on this server yet — yours would be the first. It takes a username and a password, nothing else.",
                  "de":"Auf diesem Server gibt es noch kein Konto — deines wäre das erste. Es braucht einen Benutzernamen und ein Passwort, sonst nichts.",
                  "tr":"Bu sunucuda henüz hiç hesap yok — seninki ilki olur. Bir kullanıcı adı ve bir şifre yeter, başka bir şey değil."},
 "ob_s1":        {"en":"Create an account","de":"Konto anlegen","tr":"Hesap aç"},
 "ob_s2":        {"en":"Get the game","de":"Spiel holen","tr":"Oyunu al"},
 "ob_s3":        {"en":"Play","de":"Spielen","tr":"Oyna"},
 "ob_go":        {"en":"Create the first account","de":"Erstes Konto anlegen","tr":"İlk hesabı oluştur"},
 "admin_hint_local":{"en":"This server only listens to this computer, so there is no passphrase to type.",
                  "de":"Dieser Server lauscht nur auf diesem Computer, es gibt also keine Passphrase einzugeben.",
                  "tr":"Bu sunucu yalnızca bu bilgisayarı dinliyor, bu yüzden girilecek bir parola yok."},
 "admin_open":   {"en":"🛠️ Manage the server","de":"🛠️ Server verwalten","tr":"🛠️ Sunucuyu yönet"},
 "back_front":   {"en":"← Front page","de":"← Startseite","tr":"← Ana sayfa"},
 "dl_local_t":   {"en":"The game is on this computer","de":"Das Spiel liegt auf diesem Computer","tr":"Oyun bu bilgisayarda"},
 "dl_local":     {"en":"Nothing to download: this server runs on the machine you are sitting at, so the game was unpacked here directly. Look for <b>Metin2 Singleplayer</b> on your Desktop and start it from there.",
                  "de":"Nichts herunterzuladen: Dieser Server läuft auf dem Rechner, an dem du sitzt, das Spiel wurde also gleich hier ausgepackt. Auf dem Desktop findest du <b>Metin2 Singleplayer</b> — von dort startest du es.",
                  "tr":"İndirilecek bir şey yok: bu sunucu şu an başında oturduğun makinede çalışıyor, oyun da doğrudan buraya açıldı. Masaüstünde <b>Metin2 Singleplayer</b> kısayolunu bul ve oradan başlat."},
 "dl_local_w":   {"en":"Still unpacking. It is well over a gigabyte, so give it a few minutes — the shortcut appears on the Desktop when it is done.",
                  "de":"Wird noch ausgepackt. Es ist deutlich über ein Gigabyte, gib ihm also ein paar Minuten — die Verknüpfung erscheint auf dem Desktop, sobald es fertig ist.",
                  "tr":"Hâlâ açılıyor. Bir gigabayttan epey büyük, birkaç dakika ver — bitince kısayol masaüstünde belirir."},
 # --- the operator's orientation, shown once they are logged in ---
 # This is the first thing the person who installed the server sees. It exists
 # because the dashboard used to open straight onto three cards with no
 # explanation of what had just been built or what to do with it.
 "op_title":     {"en":"Your server is running","de":"Dein Server läuft","tr":"Sunucun çalışıyor"},
 "op_intro":     {"en":"Everything is up: the game, the database and this panel. The badge at the top says whether the game itself is accepting connections — if it ever reads offline while you are sure it should not, that is the first place to look.",
                  "de":"Alles läuft: das Spiel, die Datenbank und dieses Panel. Die Anzeige oben sagt dir, ob das Spiel selbst Verbindungen annimmt — falls dort einmal „offline“ steht, obwohl du sicher bist, dass es nicht so sein sollte, schau zuerst dort nach.",
                  "tr":"Her şey ayakta: oyun, veritabanı ve bu panel. Üstteki rozet oyunun bağlantı kabul edip etmediğini gösterir — bir gün „çevrimdışı“ yazıyorsa ve bundan emin değilsen, önce oraya bak."},
 "op_share":     {"en":"To let someone play, give them the address of this page. They register an account here and download the game from the same page — it already points at your server, so there is nothing for them to configure.",
                  "de":"Damit jemand spielen kann, gib ihm die Adresse dieser Seite. Er registriert sich hier und lädt das Spiel von derselben Seite — es zeigt bereits auf deinen Server, es gibt für ihn nichts einzustellen.",
                  "tr":"Birinin oynaması için ona bu sayfanın adresini ver. Hesabını burada açar ve oyunu aynı sayfadan indirir — oyun zaten senin sunucunu gösteriyor, ayarlaması gereken bir şey yok."},
 # Shown instead of op_share when the installer reported a loopback-only setup.
 "op_local_t":   {"en":"This server is for you alone","de":"Dieser Server ist nur für dich","tr":"Bu sunucu yalnızca sana ait"},
 "op_local":     {"en":"This was installed as a local server, so everything listens on this computer only. Nobody else can join — not over the internet, and not from another device on the same network. No port was opened and no firewall rule was created. Register an account here, download the game from this page, and play.",
                  "de":"Das hier wurde als lokaler Server installiert, es lauscht also alles ausschließlich auf diesem Computer. Niemand sonst kann mitspielen — weder über das Internet noch von einem anderen Gerät im selben Netzwerk. Es wurde kein Port geöffnet und keine Firewallregel angelegt. Registriere hier ein Konto, lade das Spiel von dieser Seite und spiel los.",
                  "tr":"Bu, yerel bir sunucu olarak kuruldu; yani her şey yalnızca bu bilgisayarda dinliyor. Başka kimse katılamaz — ne internet üzerinden ne de aynı ağdaki başka bir cihazdan. Hiçbir port açılmadı ve hiçbir güvenlik duvarı kuralı oluşturulmadı. Buradan bir hesap aç, oyunu bu sayfadan indir ve oyna."},
 "op_local_hint":{"en":"If you later want friends to play, do not open ports on your home router: that hands your home address to every player, your upload speed becomes the bottleneck, and the server is gone whenever this computer is. Rent a small Linux server instead and run the installer there — the project's documentation has the one command for it.",
                  "de":"Wenn später Freunde mitspielen sollen, öffne keine Ports an deinem Heimrouter: Das gibt deine Heimadresse an jeden Spieler weiter, dein Upload wird zum Flaschenhals, und der Server ist weg, sobald dieser Rechner aus ist. Miete stattdessen einen kleinen Linux-Server und führe den Installer dort aus — der eine Befehl dafür steht in der Dokumentation des Projekts.",
                  "tr":"İleride arkadaşlarının da oynamasını istersen, ev yönlendiricinde port açma: bu, ev adresini her oyuncuya verir, yükleme hızın darboğaz olur ve bu bilgisayar kapandığında sunucu da gider. Bunun yerine küçük bir Linux sunucu kirala ve kurulumu orada çalıştır — bunun tek komutu projenin belgelerinde."},
 "op_rates":     {"en":"Rates decide how fast the whole server plays: experience, item drops and yang. 100% is the game exactly as it shipped. Saving restarts the game for well under a minute, so players are briefly disconnected.",
                  "de":"Die Raten bestimmen, wie schnell sich der ganze Server spielt: Erfahrung, Item-Drops und Yang. 100 % ist das Spiel genau so, wie es ausgeliefert wurde. Beim Speichern startet das Spiel für deutlich unter einer Minute neu, Spieler fliegen also kurz raus.",
                  "tr":"Oranlar tüm sunucunun ne kadar hızlı oynandığını belirler: tecrübe, eşya düşüşü ve yang. %100, oyunun çıktığı hâlidir. Kaydettiğinde oyun bir dakikadan çok kısa süre yeniden başlar, oyuncular kısa süre düşer."},
 "op_players":   {"en":"Under Players you find every character with the account it belongs to. Open one to give items or yang, or to set a level. Those go straight into the database, so the player has to log out and back in before they see them.",
                  "de":"Unter „Spieler“ findest du jeden Charakter mit dem Konto, zu dem er gehört. Öffne einen, um Gegenstände oder Yang zu geben oder ein Level zu setzen. Das geht direkt in die Datenbank — der Spieler muss sich also aus- und wieder einloggen, bevor er es sieht.",
                  "tr":"„Oyuncular“ altında her karakteri, ait olduğu hesapla birlikte görürsün. Eşya ya da yang vermek veya seviye ayarlamak için birini aç. Bunlar doğrudan veritabanına yazılır, yani oyuncunun görmesi için çıkıp yeniden girmesi gerekir."},
 "op_limits":    {"en":"Teleport and Running speed are different: they act on a character who is online right now, through a helper script inside the game. So they need the player to actually be logged in — for anybody who is not, the two buttons say so instead of pretending it worked.",
                  "de":"Teleportieren und Laufgeschwindigkeit sind anders: Sie wirken auf einen gerade eingeloggten Charakter, über ein Hilfsskript im Spiel. Sie brauchen also einen Spieler, der wirklich online ist — bei allen anderen sagen die beiden Schaltflächen das, statt Erfolg vorzutäuschen.",
                  "tr":"Işınla ve Koşma hızı farklıdır: o anda çevrimiçi olan bir karaktere, oyunun içindeki bir yardımcı betik üzerinden etki ederler. Yani oyuncunun gerçekten oyunda olması gerekir — olmayanlar için iki düğme de başarılı gibi davranmak yerine bunu söyler."},
 "op_forgot":    {"en":"When a player forgets their password, you make them a reset link below. It works once and expires after a day — you never see or set their password yourself.",
                  "de":"Wenn ein Spieler sein Passwort vergisst, erzeugst du ihm unten einen Reset-Link. Er funktioniert einmal und verfällt nach einem Tag — du siehst oder setzt sein Passwort nie selbst.",
                  "tr":"Bir oyuncu şifresini unutursa, aşağıda ona bir sıfırlama bağlantısı oluşturursun. Bir kez çalışır ve bir gün sonra geçersiz olur — şifresini asla sen görmez ya da belirlemezsin."},
 "op_more":      {"en":"Everything else — backups, moving the server, rebuilding the client for a new address — is in the project's documentation.",
                  "de":"Alles Weitere — Sicherungen, Serverumzug, den Client für eine neue Adresse neu bauen — steht in der Dokumentation des Projekts.",
                  "tr":"Geri kalan her şey — yedekler, sunucu taşıma, yeni bir adres için istemciyi yeniden derleme — projenin belgelerinde."},
 # --- changing the admin passphrase from here ---
 "op_pp":        {"en":"The passphrase for this panel is the one the installer printed when it finished. You can pick your own below — the installer will show whichever one is current every time you run it again.",
                  "de":"Die Passphrase für dieses Panel ist die, die der Installer am Ende ausgegeben hat. Unten kannst du eine eigene wählen — der Installer zeigt dir bei jedem weiteren Lauf die jeweils aktuelle.",
                  "tr":"Bu panelin gizli kelimesi, kurulum bittiğinde yazdırdığı kelimedir. Aşağıdan kendi seçebilirsin — kurulumu her yeniden çalıştırdığında sana güncel olanı gösterir."},
 "pp_title":     {"en":"🔑 Admin passphrase","de":"🔑 Admin-Passphrase","tr":"🔑 Yönetici gizli kelimesi"},
 "pp_old":       {"en":"Current passphrase","de":"Aktuelle Passphrase","tr":"Mevcut gizli kelime"},
 "pp_new":       {"en":"New passphrase (at least {n} characters)","de":"Neue Passphrase (mindestens {n} Zeichen)","tr":"Yeni gizli kelime (en az {n} karakter)"},
 "pp_new2":      {"en":"New passphrase again","de":"Neue Passphrase wiederholen","tr":"Yeni gizli kelimeyi tekrar"},
 "pp_save":      {"en":"🔑 Change passphrase","de":"🔑 Passphrase ändern","tr":"🔑 Gizli kelimeyi değiştir"},
 "pp_local":     {"en":"This server only listens to this computer, so the panel does not ask for a passphrase. Setting one now means it is ready if you ever put this server somewhere others can reach.",
                  "de":"Dieser Server lauscht nur auf diesem Rechner, deshalb fragt das Panel gar nicht nach einer Passphrase. Wenn du jetzt eine setzt, ist sie bereit, falls du den Server später irgendwo hinstellst, wo andere ihn erreichen.",
                  "tr":"Bu sunucu yalnızca bu bilgisayarı dinliyor, bu yüzden panel gizli kelime sormuyor. Şimdi bir tane belirlersen, sunucuyu ileride başkalarının erişebileceği bir yere koyduğunda hazır olur."},
 "pp_done":      {"en":"Passphrase changed. Use the new one from now on — the installer will show it the next time you run it.",
                  "de":"Passphrase geändert. Ab jetzt gilt die neue — der Installer zeigt sie dir beim nächsten Lauf an.",
                  "tr":"Gizli kelime değiştirildi. Bundan sonra yenisi geçerli — kurulumu bir dahaki çalıştırmanda onu gösterecek."},
 "pp_done_unsaved":{"en":"Passphrase changed — use the new one from now on. It could not be written down for the installer, so make a note of it yourself.",
                  "de":"Passphrase geändert — ab jetzt gilt die neue. Sie konnte für den Installer nicht hinterlegt werden, notier sie dir also selbst.",
                  "tr":"Gizli kelime değiştirildi — bundan sonra yenisi geçerli. Kurulum için kaydedilemedi, bu yüzden kendin not al."},
 "pp_bad_old":   {"en":"That is not the current passphrase, so nothing was changed.",
                  "de":"Das ist nicht die aktuelle Passphrase, es wurde nichts geändert.",
                  "tr":"Bu, mevcut gizli kelime değil; hiçbir şey değiştirilmedi."},
 "pp_short":     {"en":"A passphrase needs at least {n} characters. Nothing was changed.",
                  "de":"Eine Passphrase braucht mindestens {n} Zeichen. Es wurde nichts geändert.",
                  "tr":"Gizli kelime en az {n} karakter olmalı. Hiçbir şey değiştirilmedi."},
 "pp_mismatch":  {"en":"The two new passphrases are not the same, so nothing was changed.",
                  "de":"Die beiden neuen Passphrasen sind nicht gleich, es wurde nichts geändert.",
                  "tr":"İki yeni gizli kelime aynı değil; hiçbir şey değiştirilmedi."},
 "pp_failed":    {"en":"The passphrase could not be saved, so it is unchanged. The panel's settings file is not writable.",
                  "de":"Die Passphrase konnte nicht gespeichert werden, sie bleibt also unverändert. Die Einstellungsdatei des Panels ist nicht beschreibbar.",
                  "tr":"Gizli kelime kaydedilemedi, bu yüzden değişmedi. Panelin ayar dosyası yazılabilir değil."},
 "pp_env":       {"en":"This panel takes its passphrase from its environment, not from its settings file, so it cannot be changed here. Change it where that value is set.",
                  "de":"Dieses Panel bezieht seine Passphrase aus der Umgebung, nicht aus seiner Einstellungsdatei — hier lässt sie sich deshalb nicht ändern. Ändere sie dort, wo dieser Wert gesetzt wird.",
                  "tr":"Bu panel gizli kelimesini ayar dosyasından değil ortam değişkeninden alıyor, bu yüzden burada değiştirilemez. Bu değerin ayarlandığı yerden değiştir."},
 "tip_pp":       {"en":"Changes the passphrase for this admin panel. It does not affect any game account.",
                  "de":"Ändert die Passphrase für dieses Admin-Panel. Auf Spielkonten hat das keine Auswirkung.",
                  "tr":"Bu yönetim panelinin gizli kelimesini değiştirir. Oyun hesaplarını etkilemez."},
 "op_hide":      {"en":"Got it — hide this","de":"Verstanden — ausblenden","tr":"Anladım — gizle"},
 "op_show":      {"en":"Show the introduction again","de":"Einführung wieder anzeigen","tr":"Tanıtımı yeniden göster"},
 # --- admin-made password reset links ---
 "reset_title":  {"en":"Password reset link","de":"Passwort-Reset-Link","tr":"Şifre sıfırlama bağlantısı"},
 "reset_hint":   {"en":"A player who forgot their password writes to you (the address is on the front page). Type their username here, send them the link this creates, and they choose a new password themselves. Each link works once and expires after 24 hours; making a new one cancels the old.",
                  "de":"Ein Spieler, der sein Passwort vergessen hat, schreibt dir (die Adresse steht auf der Startseite). Trage hier seinen Benutzernamen ein, schicke ihm den erzeugten Link, und er wählt selbst ein neues Passwort. Jeder Link funktioniert einmal und verfällt nach 24 Stunden; ein neuer Link macht den alten ungültig.",
                  "tr":"Şifresini unutan oyuncu sana yazar (adres ana sayfada). Buraya kullanıcı adını yaz, oluşan bağlantıyı ona gönder, yeni şifresini kendisi seçer. Her bağlantı bir kez çalışır ve 24 saat sonra geçersiz olur; yenisi eskisini iptal eder."},
 "reset_user_ph":{"en":"Player's username","de":"Benutzername des Spielers","tr":"Oyuncunun kullanıcı adı"},
 "reset_make":   {"en":"🔗 Create reset link","de":"🔗 Reset-Link erstellen","tr":"🔗 Bağlantı oluştur"},
 "reset_noacc":  {"en":"There is no account with that username.","de":"Es gibt kein Konto mit diesem Benutzernamen.","tr":"Bu kullanıcı adıyla bir hesap yok."},
 "reset_made":   {"en":"Send this link to the player — it works once and expires in 24 hours:",
                  "de":"Schick diesen Link an den Spieler — er funktioniert einmal und verfällt in 24 Stunden:",
                  "tr":"Bu bağlantıyı oyuncuya gönder — bir kez çalışır ve 24 saat sonra geçersiz olur:"},
 "reset_set_title":{"en":"Set a new password","de":"Neues Passwort setzen","tr":"Yeni şifre belirle"},
 "reset_for":    {"en":"New password for account","de":"Neues Passwort für das Konto","tr":"Yeni şifre belirlenecek hesap:"},
 "reset_ph1":    {"en":"New password (at least 6 characters)","de":"Neues Passwort (mindestens 6 Zeichen)","tr":"Yeni şifre (en az 6 karakter)"},
 "reset_ph2":    {"en":"New password again","de":"Neues Passwort wiederholen","tr":"Yeni şifre (tekrar)"},
 "reset_set_btn":{"en":"🔒 Save new password","de":"🔒 Neues Passwort speichern","tr":"🔒 Yeni şifreyi kaydet"},
 "reset_bad_link":{"en":"This link is not valid any more — it was already used, has expired, or was replaced by a newer one. Ask the admin for a fresh link.",
                  "de":"Dieser Link ist nicht mehr gültig — er wurde schon benutzt, ist abgelaufen oder wurde durch einen neueren ersetzt. Bitte den Admin um einen frischen Link.",
                  "tr":"Bu bağlantı artık geçerli değil — zaten kullanıldı, süresi doldu ya da yenisiyle değiştirildi. Yöneticiden yeni bir bağlantı iste."},
 "reset_short":  {"en":"The new password must be at least 6 characters. 🙂","de":"Das neue Passwort muss mindestens 6 Zeichen haben. 🙂","tr":"Yeni şifre en az 6 karakter olmalı. 🙂"},
 "reset_mismatch":{"en":"The two passwords don't match — try again. 🙂","de":"Die beiden Passwörter stimmen nicht überein — versuch es noch einmal. 🙂","tr":"İki şifre birbirini tutmuyor — tekrar dene. 🙂"},
 "reset_done":   {"en":"Your password has been changed — you can log into the game with it right away. 🎉",
                  "de":"Dein Passwort wurde geändert — du kannst dich damit sofort im Spiel anmelden. 🎉",
                  "tr":"Şifren değiştirildi — hemen oyuna girebilirsin. 🎉"},
 "tip_reset":    {"en":"Creates a one-time link that lets this player set a new password themselves. Nothing changes on the account until the link is used.",
                  "de":"Erzeugt einen Einmal-Link, mit dem der Spieler selbst ein neues Passwort setzt. Am Konto ändert sich nichts, bis der Link benutzt wird.",
                  "tr":"Oyuncunun kendisinin yeni şifre belirlemesini sağlayan tek kullanımlık bir bağlantı oluşturur. Bağlantı kullanılana dek hesapta hiçbir şey değişmez."},
 "dl_limit_title":{"en":"Download limit reached","de":"Download-Limit erreicht","tr":"İndirme sınırına ulaşıldı"},
 # Shown when the whole server has hit its daily ceiling rather than the visitor
 # -- otherwise the reader concludes they did something wrong, and they didn't.
 "dl_limit_all": {"en":"The game has been downloaded the maximum number of times across the whole server today. This is not about you — somebody has to be the one who arrives after the last slot. It frees up again in about {h} h, and an interrupted download can always be resumed, which costs nothing.",
                  "de":"Das Spiel wurde heute serverweit schon so oft heruntergeladen, wie erlaubt ist. Das liegt nicht an dir — irgendwer muss der sein, der nach dem letzten freien Platz ankommt. In etwa {h} Std. wird wieder einer frei. Ein abgebrochener Download lässt sich jederzeit fortsetzen, das kostet nichts.",
                  "tr":"Oyun bugün sunucu genelinde izin verilen en yüksek sayıda indirildi. Bu senden kaynaklanmıyor — birinin son boş yerden sonra gelmesi gerekiyordu. Yaklaşık {h} saat içinde yeniden yer açılır. Yarım kalan bir indirme her zaman kaldığı yerden sürdürülebilir, bu sınırdan sayılmaz."},
 "dl_limit":     {"en":"The game was already downloaded 3 times from your address in the last 24 hours — that is the limit, so the server's bandwidth stays free for playing. Please try again in about {h} h. An interrupted download can always be resumed, that costs nothing.",
                  "de":"Das Spiel wurde von deiner Adresse in den letzten 24 Stunden schon 3-mal heruntergeladen — mehr geht nicht, damit die Bandbreite des Servers zum Spielen frei bleibt. Versuche es in etwa {h} Std. wieder. Ein abgebrochener Download lässt sich jederzeit fortsetzen, das kostet nichts.",
                  "tr":"Oyun son 24 saatte senin adresinden zaten 3 kez indirildi — sunucunun bant genişliği oyuna kalsın diye sınır bu. Yaklaşık {h} saat sonra tekrar dene. Yarım kalan bir indirme her zaman kaldığı yerden sürdürülebilir, bu sınırdan sayılmaz."},

 # --- mouseover explanations (title="..." on the elements themselves) ---
 "tip_lang":     {"en":"Switch the language of this panel. Nothing in the game changes.",
                  "de":"Ändert die Sprache dieses Panels. Im Spiel ändert sich nichts.",
                  "tr":"Bu panelin dilini değiştirir. Oyunda hiçbir şey değişmez."},
 "tip_logout":   {"en":"End your admin session on this panel. Your game account is not affected.",
                  "de":"Beendet deine Admin-Sitzung in diesem Panel. Dein Spiel-Konto ist davon nicht betroffen.",
                  "tr":"Bu paneldeki yönetici oturumunu kapatır. Oyun hesabın etkilenmez."},
 "tip_passphrase":{"en":"The admin passphrase chosen when the server was installed. Players do not need this — only the person running the server.",
                  "de":"Die Admin-Passphrase, die bei der Installation des Servers gewählt wurde. Spieler brauchen sie nicht — nur wer den Server betreibt.",
                  "tr":"Sunucu kurulurken seçilen yönetici parolası. Oyuncuların buna ihtiyacı yok — sadece sunucuyu işleten kişinin."},
 "tip_login":    {"en":"Opens the admin area. Only works with the admin passphrase, not with your game password.",
                  "de":"Öffnet den Admin-Bereich. Funktioniert nur mit der Admin-Passphrase, nicht mit deinem Spiel-Passwort.",
                  "tr":"Yönetici alanını açar. Sadece yönetici parolasıyla çalışır, oyun şifrenle değil."},
 "tip_download": {"en":"Downloads the complete game, around 1.2 GB. The server address is already filled in, so you do not have to configure anything — unpack it and start the game. At most 3 downloads per day; resuming an interrupted one is always free.",
                  "de":"Lädt das komplette Spiel herunter, etwa 1,2 GB. Die Serveradresse ist schon eingetragen, du musst nichts einstellen — entpacken und starten. Höchstens 3 Downloads pro Tag; einen abgebrochenen fortzusetzen ist immer frei.",
                  "tr":"Oyunun tamamını indirir, yaklaşık 1,2 GB. Sunucu adresi zaten girili, hiçbir ayar yapman gerekmiyor — çıkart ve başlat. Günde en fazla 3 indirme; yarım kalanı sürdürmek her zaman serbest."},
 "tip_create_acc":{"en":"Creates the account you log into the game with. It is separate from this panel and takes about half a minute.",
                  "de":"Erstellt das Konto, mit dem du dich im Spiel anmeldest. Es ist unabhängig von diesem Panel und dauert etwa eine halbe Minute.",
                  "tr":"Oyuna giriş yapacağın hesabı oluşturur. Bu panelden bağımsızdır ve yarım dakika sürer."},
 "tip_my_acc":   {"en":"Log into your game account here to change its password.",
                  "de":"Melde dich hier mit deinem Spiel-Konto an, um dessen Passwort zu ändern.",
                  "tr":"Oyun hesabının şifresini değiştirmek için burada giriş yap."},
 "tip_players":  {"en":"Every character that exists on this server. Characters show up after someone has logged in and created one.",
                  "de":"Alle Charaktere, die es auf diesem Server gibt. Sie erscheinen, sobald jemand sich eingeloggt und einen erstellt hat.",
                  "tr":"Bu sunucudaki tüm karakterler. Biri giriş yapıp karakter oluşturduktan sonra burada görünürler."},
 "tip_player":   {"en":"Open this character to give items or yang, set the level, teleport them or change their speed.",
                  "de":"Öffnet diesen Charakter, um Gegenstände oder Yang zu geben, das Level zu setzen, ihn zu teleportieren oder seine Geschwindigkeit zu ändern.",
                  "tr":"Bu karakteri açar: eşya veya yang verebilir, seviyesini ayarlayabilir, ışınlayabilir veya hızını değiştirebilirsin."},
 "tip_search_item":{"en":"Type part of a name, or an item number if you know it. Around 9,800 items are searchable.",
                  "de":"Tippe einen Teil des Namens oder die Item-Nummer, falls du sie kennst. Rund 9.800 Gegenstände sind durchsuchbar.",
                  "tr":"Adının bir kısmını yaz, ya da biliyorsan eşya numarasını. Yaklaşık 9.800 eşya aranabilir."},
 "tip_category": {"en":"Narrows the search to one kind of item, so you do not have to scroll past everything else.",
                  "de":"Schränkt die Suche auf eine Art von Gegenstand ein, damit du nicht an allem anderen vorbeiscrollen musst.",
                  "tr":"Aramayı tek bir eşya türüyle sınırlar, böylece diğer her şeyi kaydırmak zorunda kalmazsın."},
 "tip_qty":      {"en":"How many of this item to give. One stack holds at most 65,535.",
                  "de":"Wie viele von diesem Gegenstand gegeben werden. In einen Stapel passen höchstens 65.535.",
                  "tr":"Bu eşyadan kaç adet verilecek. Bir yığında en fazla 65.535 durur."},
 "tip_send_item":{"en":"Puts the item into the character's inventory. If they are offline it is waiting at their next login.",
                  "de":"Legt den Gegenstand in das Inventar des Charakters. Ist er offline, liegt er beim nächsten Login bereit.",
                  "tr":"Eşyayı karakterin çantasına koyar. Çevrimdışıysa bir sonraki girişinde onu bekliyor olur."},
 "tip_amount":   {"en":"How much yang to add. Enter a negative number to take yang away instead.",
                  "de":"Wie viel Yang hinzukommt. Gib eine negative Zahl ein, um stattdessen Yang abzuziehen.",
                  "tr":"Ne kadar yang ekleneceği. Yang almak için eksi bir sayı gir."},
 "tip_level":    {"en":"Sets the character straight to this level. It does not add levels, it replaces the current one.",
                  "de":"Setzt den Charakter direkt auf dieses Level. Es wird nicht dazugezählt, sondern ersetzt.",
                  "tr":"Karakteri doğrudan bu seviyeye ayarlar. Seviye eklemez, mevcut olanı değiştirir."},
 "tip_teleport": {"en":"Moves the character to another map. This only works while they are actually in game.",
                  "de":"Bewegt den Charakter auf eine andere Karte. Das geht nur, solange er wirklich im Spiel ist.",
                  "tr":"Karakteri başka bir haritaya taşır. Bu sadece gerçekten oyundayken çalışır."},
 "tip_speed":    {"en":"Changes how fast the character runs. 100 is normal; this only works while they are in game.",
                  "de":"Ändert, wie schnell der Charakter läuft. 100 ist normal; das geht nur, solange er im Spiel ist.",
                  "tr":"Karakterin ne kadar hızlı koştuğunu değiştirir. 100 normaldir; sadece oyundayken çalışır."},
 "tip_gm":       {"en":"Gives this character the in-game admin commands, typed into the chat box as /command. Granting works immediately; taking it away needs the player to log out and back in.",
                  "de":"Gibt diesem Charakter die Admin-Befehle im Spiel, die als /befehl ins Chatfenster getippt werden. Vergeben wirkt sofort; Entziehen wirkt erst, wenn der Spieler sich aus- und wieder einloggt.",
                  "tr":"Bu karaktere, sohbet kutusuna /komut olarak yazılan oyun içi yönetici komutlarını verir. Vermek hemen etkilidir; almak için oyuncunun çıkıp tekrar girmesi gerekir."},
 "tip_rates":    {"en":"Experience, item drops and yang for the whole server. Saving restarts the game for under a minute.",
                  "de":"Erfahrung, Item-Drops und Yang für den ganzen Server. Beim Speichern startet das Spiel für weniger als eine Minute neu.",
                  "tr":"Tüm sunucu için tecrübe, eşya düşme oranı ve yang. Kaydettiğinde oyun bir dakikadan kısa bir süre yeniden başlar."},
 "tip_delcode":  {"en":"Seven digits the game asks for when you delete a character. Pick something you will remember.",
                  "de":"Sieben Ziffern, nach denen das Spiel fragt, wenn du einen Charakter löschst. Wähle etwas, das du dir merkst.",
                  "tr":"Bir karakteri silerken oyunun soracağı yedi rakam. Hatırlayacağın bir şey seç."},
 # --- version, patch log, updates -------------------------------------------
 "ver_label":    {"en":"Version","de":"Version","tr":"Sürüm"},
 "ver_unknown":  {"en":"unknown","de":"unbekannt","tr":"bilinmiyor"},
 "ver_unknown_why":{"en":"This build does not carry a version file, so the panel cannot say which one it is — and will not guess. Update checks stay off until it can.",
                  "de":"Dieser Build enthält keine Versionsdatei, also kann das Panel nicht sagen, welche Version es ist — und rät nicht. Solange bleibt die Update-Prüfung ohne Wirkung.",
                  "tr":"Bu yapıda sürüm dosyası yok, bu yüzden panel hangi sürüm olduğunu söyleyemez — ve tahmin etmez. O zamana kadar güncelleme kontrolü sonuç vermez."},
 "pl_nav":       {"en":"Patch log","de":"Änderungsprotokoll","tr":"Sürüm notları"},
 "pl_dash_hint": {"en":"What changed in this build, and what a newer one would change.",
                  "de":"Was sich in diesem Build geändert hat — und was ein neuerer ändern würde.",
                  "tr":"Bu yapıda neler değişti ve daha yenisi neleri değiştirir."},
 "tip_patchlog": {"en":"The project's changelog: the version you are running, and — when there is one — the version that has been published since.",
                  "de":"Das Änderungsprotokoll des Projekts: die Version, die du fährst, und — falls vorhanden — die inzwischen veröffentlichte.",
                  "tr":"Projenin değişiklik günlüğü: çalıştırdığın sürüm ve — varsa — o zamandan beri yayımlanan sürüm."},
 "pl_none":      {"en":"The changelog is not part of this build, so there is nothing to show here.",
                  "de":"Das Änderungsprotokoll ist in diesem Build nicht enthalten, hier gibt es also nichts zu zeigen.",
                  "tr":"Değişiklik günlüğü bu yapıda yok, bu yüzden burada gösterilecek bir şey yok."},
 "upd_avail_t":  {"en":"A newer version has been published","de":"Es wurde eine neuere Version veröffentlicht","tr":"Daha yeni bir sürüm yayımlandı"},
 "upd_avail":    {"en":"You are running {cur}. {new} is available.",
                  "de":"Bei dir läuft {cur}. Verfügbar ist {new}.",
                  "tr":"Sende {cur} çalışıyor. {new} mevcut."},
 "upd_avail_short":{"en":"update available","de":"Update verfügbar","tr":"güncelleme var"},
 "upd_see":      {"en":"See what it brings","de":"Ansehen, was es bringt","tr":"Neler getirdiğine bak"},
 "pl_card_t":    {"en":"Version and changes","de":"Version und Änderungen","tr":"Sürüm ve değişiklikler"},
 "pl_new_t":     {"en":"What an update would bring","de":"Was ein Update bringen würde","tr":"Güncelleme ne getirir"},
 "pl_have_t":    {"en":"What you are running","de":"Was bei dir läuft","tr":"Çalıştırdığın sürüm"},
 "pl_check":     {"en":"🔄 Check for the latest version","de":"🔄 Nach der neuesten Version suchen","tr":"🔄 En son sürümü kontrol et"},
 "pl_check_new": {"en":"A newer version is available: {new}.","de":"Eine neuere Version ist verfügbar: {new}.","tr":"Daha yeni bir sürüm var: {new}."},
 "pl_check_ok":  {"en":"Checked. You already have the newest published version.","de":"Geprüft. Du hast bereits die neueste veröffentlichte Version.","tr":"Kontrol edildi. Zaten en yeni yayımlanan sürüme sahipsin."},
 "pl_check_bad": {"en":"The update server could not be reached. Nothing on your server changed.","de":"Zugriff auf den Update-Server war nicht möglich. An deinem Server hat sich nichts geändert.","tr":"Güncelleme sunucusuna ulaşılamadı. Sunucunda hiçbir şey değişmedi."},
 "pl_check_off": {"en":"The update check is switched off, so this asked nothing. Set M2_UPDATE_CHECK=1 in .env to turn it back on.","de":"Die Update-Prüfung ist ausgeschaltet, es wurde also nichts abgefragt. Setze M2_UPDATE_CHECK=1 in .env, um sie wieder einzuschalten.","tr":"Güncelleme kontrolü kapalı, bu yüzden hiçbir sorgu yapılmadı. Yeniden açmak için .env içinde M2_UPDATE_CHECK=1 ayarla."},
 "pl_check_wait":{"en":"Just checked a moment ago — give it a minute.","de":"Gerade eben schon geprüft — gib ihm eine Minute.","tr":"Az önce kontrol edildi — bir dakika bekle."},
 "pl_open":      {"en":"📜 Open the patch log","de":"📜 Patchlog öffnen","tr":"📜 Sürüm notlarını aç"},
 "upd_none":     {"en":"This is the newest published version.","de":"Das ist die neueste veröffentlichte Version.","tr":"Bu, yayımlanan en yeni sürüm."},
 "upd_never":    {"en":"Not checked yet — the first check happens a couple of minutes after the panel starts.",
                  "de":"Noch nicht geprüft — die erste Prüfung läuft ein paar Minuten nach dem Start des Panels.",
                  "tr":"Henüz kontrol edilmedi — ilk kontrol panel başladıktan birkaç dakika sonra yapılır."},
 "upd_failed":   {"en":"The update server could not be reached.",
                  "de":"Zugriff auf den Update-Server war nicht möglich.",
                  "tr":"Güncelleme sunucusuna ulaşılamadı."},
 "upd_checked":  {"en":"Last checked","de":"Zuletzt geprüft","tr":"Son kontrol"},
 "upd_off_t":    {"en":"The update check is switched off","de":"Die Update-Prüfung ist ausgeschaltet","tr":"Güncelleme kontrolü kapalı"},
 "upd_off":      {"en":"This panel is not contacting anything at all. You will not be told when a newer version appears; look at the project page when you want to know. To switch it back on, set M2_UPDATE_CHECK=1 in .env and restart the panel.",
                  "de":"Dieses Panel kontaktiert überhaupt nichts. Du wirst nicht erfahren, wenn eine neuere Version erscheint; schau auf der Projektseite nach, wenn du es wissen willst. Zum Einschalten: M2_UPDATE_CHECK=1 in der .env setzen und das Panel neu starten.",
                  "tr":"Bu panel hiçbir yere bağlanmıyor. Yeni bir sürüm çıktığında haber verilmez; öğrenmek istediğinde proje sayfasına bak. Açmak için .env dosyasında M2_UPDATE_CHECK=1 yap ve paneli yeniden başlat."},
 "upd_phone_t":  {"en":"This page reaches the internet","de":"Diese Seite geht ins Internet","tr":"Bu sayfa internete çıkıyor"},
 "upd_phone":    {"en":"Once a day, in the background, the panel fetches two text files from the project's repository on GitHub: the published version number, and — only when it is newer than yours — the changelog. That is the only thing in this whole project that contacts anything by itself, and it means GitHub sees your server's address and the time of the request, roughly once a day, the same as any visit to a web page would. Nothing about your server, your players or your database is sent, nothing that comes back is executed, and no page ever waits for it.",
                  "de":"Einmal am Tag holt das Panel im Hintergrund zwei Textdateien aus dem Projekt-Repository auf GitHub: die veröffentlichte Versionsnummer und — nur wenn sie neuer ist als deine — das Änderungsprotokoll. Das ist das Einzige in diesem ganzen Projekt, das von sich aus irgendwo Kontakt aufnimmt, und es bedeutet: GitHub sieht die Adresse deines Servers und den Zeitpunkt der Anfrage, ungefähr einmal täglich, genau wie bei jedem Aufruf einer Webseite. Nichts über deinen Server, deine Spieler oder deine Datenbank wird übertragen, nichts davon wird ausgeführt, und keine Seite wartet darauf.",
                  "tr":"Panel günde bir kez, arka planda, GitHub'daki proje deposundan iki metin dosyası alır: yayımlanan sürüm numarası ve — yalnızca seninkinden yeniyse — değişiklik günlüğü. Bu, tüm projede kendi başına bir yere bağlanan tek şeydir ve şu anlama gelir: GitHub, günde yaklaşık bir kez, sunucunun adresini ve isteğin zamanını görür — herhangi bir web sayfasını açmakla aynı. Sunucun, oyuncuların veya veritabanın hakkında hiçbir şey gönderilmez, gelen hiçbir şey çalıştırılmaz ve hiçbir sayfa bunu beklemez."},
 "upd_phone_off":{"en":"To stop it: set M2_UPDATE_CHECK=0 in .env and restart the panel. Everything else keeps working exactly as it does now.",
                  "de":"So schaltest du es ab: M2_UPDATE_CHECK=0 in der .env setzen und das Panel neu starten. Alles andere funktioniert genau wie bisher.",
                  "tr":"Kapatmak için: .env dosyasında M2_UPDATE_CHECK=0 yap ve paneli yeniden başlat. Diğer her şey aynen çalışmaya devam eder."},
 # --- installing an update from the panel ------------------------------------
 "upd_nav":      {"en":"Update this server","de":"Diesen Server aktualisieren","tr":"Bu sunucuyu güncelle"},
 "upd_page_t":   {"en":"Install the update","de":"Update installieren","tr":"Güncellemeyi kur"},
 "upd_open":     {"en":"⬆️ Install it from here","de":"⬆️ Von hier installieren","tr":"⬆️ Buradan kur"},
 "upd_warn_t":   {"en":"Before you start","de":"Bevor du startst","tr":"Başlamadan önce"},
 "upd_warn":     {"en":"The game is rebuilt and restarted, so everyone playing is disconnected and cannot log back in until it is up again. Usually two or three minutes; longer when the game itself has to be recompiled. Accounts, characters, items and guilds are not touched — they live in the database, and nothing in an update goes near it.",
                  "de":"Das Spiel wird neu gebaut und neu gestartet, alle Spielenden fliegen dabei raus und kommen erst wieder rein, wenn es läuft. Meist zwei bis drei Minuten; länger, wenn das Spiel selbst neu übersetzt werden muss. Konten, Charaktere, Gegenstände und Gilden bleiben unangetastet — sie liegen in der Datenbank, und kein Update fasst sie an.",
                  "tr":"Oyun yeniden derlenip yeniden başlatılır; oynayan herkesin bağlantısı kesilir ve sunucu geri gelene kadar giriş yapamazlar. Genellikle iki üç dakika; oyunun kendisi yeniden derlenmesi gerekiyorsa daha uzun. Hesaplar, karakterler, eşyalar ve loncalar etkilenmez — onlar veritabanında durur ve hiçbir güncelleme oraya dokunmaz."},
 "upd_warn_panel":{"en":"The panel restarts too, so this page will lose contact for a minute near the end. Leave it open — it picks up again by itself and shows how it finished.",
                  "de":"Auch das Panel startet neu, diese Seite verliert also gegen Ende kurz den Kontakt. Lass sie offen — sie meldet sich von selbst zurück und zeigt, wie es ausgegangen ist.",
                  "tr":"Panel de yeniden başlar, bu yüzden bu sayfa sona doğru bir dakikalığına bağlantıyı kaybeder. Açık bırak — kendiliğinden geri döner ve nasıl bittiğini gösterir."},
 "upd_start":    {"en":"Start the update","de":"Update starten","tr":"Güncellemeyi başlat"},
 "upd_started":  {"en":"The update has been requested. It starts within a few seconds.",
                  "de":"Das Update wurde angefordert. Es startet in wenigen Sekunden.",
                  "tr":"Güncelleme istendi. Birkaç saniye içinde başlar."},
 "upd_no_watcher_t":{"en":"The updater is not running","de":"Der Updater läuft nicht","tr":"Güncelleyici çalışmıyor"},
 "upd_no_watcher":{"en":"Updating from the panel is switched on, but the small helper that carries it out is not there — so the button would do nothing. Start it on the server with:",
                  "de":"Updates aus dem Panel sind eingeschaltet, aber das kleine Hilfsprogramm, das sie ausführt, läuft nicht — der Knopf würde also nichts tun. Starte es auf dem Server mit:",
                  "tr":"Panelden güncelleme açık, ama işi yapan küçük yardımcı çalışmıyor — yani düğme hiçbir şey yapmazdı. Sunucuda şununla başlat:"},
 # Deliberately does not name a shell: the same page is served by a server on a
 # Linux VPS and by one on somebody's PC, and the command shown underneath is
 # whichever of the two installed this machine.
 "upd_manual":   {"en":"Run the same command you used to install this server. It fetches the published version, rebuilds and restarts:",
                  "de":"Führe denselben Befehl aus, mit dem du diesen Server installiert hast. Er holt die veröffentlichte Version, baut neu und startet neu:",
                  "tr":"Bu sunucuyu kurarken kullandığın komutu tekrar çalıştır. Yayımlanan sürümü indirir, yeniden derler ve yeniden başlatır:"},
 "upd_manual_keeps":{"en":"Your database is not touched: accounts, characters, items and guilds all stay, and so do your settings. Before anything changes it shows which version you have and which one is published, and asks. Players are disconnected while the server restarts, usually for a minute or two.",
                  "de":"Deine Datenbank wird nicht angefasst: Konten, Charaktere, Gegenstände und Gilden bleiben, ebenso deine Einstellungen. Bevor sich etwas ändert, zeigt er dir, welche Version du hast und welche veröffentlicht ist, und fragt nach. Während des Neustarts fliegen Spieler kurz raus, meist für ein bis zwei Minuten.",
                  "tr":"Veritabanına dokunulmaz: hesaplar, karakterler, eşyalar ve loncalar kalır, ayarların da öyle. Herhangi bir şey değişmeden önce hangi sürümde olduğunu ve hangisinin yayımlandığını gösterir ve sorar. Sunucu yeniden başlarken oyuncular kısa süre düşer, genelde bir iki dakika."},
 "upd_progress": {"en":"Progress","de":"Fortschritt","tr":"İlerleme"},
 "upd_waiting":  {"en":"Waiting for the updater to pick this up…","de":"Warte darauf, dass der Updater das aufnimmt…","tr":"Güncelleyicinin bunu almasını bekliyorum…"},
 "upd_lost":     {"en":"No contact with the panel — it is probably restarting. This page keeps trying.",
                  "de":"Kein Kontakt zum Panel — es startet vermutlich gerade neu. Diese Seite versucht es weiter.",
                  "tr":"Panelle bağlantı yok — muhtemelen yeniden başlıyor. Bu sayfa denemeye devam ediyor."},
 "upd_st_queued":{"en":"Requested — the updater has not started yet.","de":"Angefordert — der Updater hat noch nicht begonnen.","tr":"İstendi — güncelleyici henüz başlamadı."},
 "upd_st_running":{"en":"Running. Do not close the server down while this is happening.","de":"Läuft. Fahre den Server währenddessen nicht herunter.","tr":"Çalışıyor. Bu sırada sunucuyu kapatma."},
 "upd_st_ok":    {"en":"Done. The server is running the new version.","de":"Fertig. Der Server läuft auf der neuen Version.","tr":"Bitti. Sunucu yeni sürümle çalışıyor."},
 "upd_st_failed":{"en":"It did not finish. Your server was left running the version it had — nothing was removed and no data was touched. The log below says where it stopped.",
                  "de":"Es wurde nicht fertig. Dein Server läuft weiter auf der bisherigen Version — nichts wurde entfernt und keine Daten angefasst. Das Protokoll unten sagt, wo es stehen geblieben ist.",
                  "tr":"Tamamlanmadı. Sunucun eski sürümüyle çalışmaya devam ediyor — hiçbir şey silinmedi, hiçbir veriye dokunulmadı. Aşağıdaki günlük nerede durduğunu söylüyor."},
 "upd_req_failed":{"en":"The request could not be written — the shared updater directory is not mounted in this container.",
                  "de":"Die Anfrage konnte nicht geschrieben werden — das gemeinsame Updater-Verzeichnis ist in diesem Container nicht eingebunden.",
                  "tr":"İstek yazılamadı — paylaşılan güncelleyici dizini bu kapsayıcıda bağlı değil."},
 "upd_not_now":  {"en":"There is nothing to install right now.","de":"Im Moment gibt es nichts zu installieren.","tr":"Şu anda kurulacak bir şey yok."},
}
# The live map can now be switched to Polish.  These shared-frame strings are
# the only values from the older panel dictionary that the map renders.
T["logout"]["pl"] = "Wyloguj"
T["back_front"]["pl"] = "← Strona główna"
T["tip_lang"]["pl"] = "Zmień język tego panelu. Język w grze pozostaje bez zmian."
T["tip_logout"]["pl"] = "Zakończ sesję administratora w panelu. Konto w grze pozostaje bez zmian."
T["about_goal"]["pl"] = "Lokalny świat Metin2 rozwijany jako hobbystyczne środowisko dla autonomicznych botów graczy."

CATS = ["all","weapon","armor","usable","ds","metin","special","other"]

def lang():
    """Chosen language, or the browser's if none was chosen yet.

    First visit: the browser says what it prefers (Accept-Language) and a
    German visitor sees German without clicking anything. Clicking a language
    in the header stores it in the session and wins from then on.
    """
    chosen = session.get("lang")
    if chosen in LANGS:
        return chosen
    if has_request_context():
        best = request.accept_languages.best_match(list(LANGS))
        if best:
            return best
    return "en"

def t(key):
    return T.get(key, {}).get(lang(), T.get(key, {}).get("en", key))

app = Flask(__name__)
app.jinja_env.globals["DISCORD"] = DISCORD_URL
app.secret_key = CONF["flask_secret"]


# Is there a reverse proxy in front of this panel? Off unless the installer
# says otherwise, so an install that predates this setting keeps behaving as it
# did.
TRUST_PROXY = bool(CONF.get("trust_proxy", False))

class _LocalProxyFix:
    """Apply X-Forwarded-* headers, but only when nginx sent them.

    The panel is reachable two ways at once: through the HTTPS reverse proxy on
    the domain name, and directly on its own port over plain HTTP. So these
    headers can also arrive straight from a visitor, and a forged
    X-Forwarded-Host would put an attacker's domain into every link the panel
    generates. Believing them needs a reason.

    The loopback test alone was the wrong reason. It is nginx that sits on the
    loopback address, not this process: the panel runs in a container and
    Docker's port forwarder rewrites the source address, so every proxied
    request arrives from the bridge gateway and the test never matched. Nothing
    announced that -- the panel quietly generated links as though it were being
    reached directly, which is how the Play in Browser button came to name the
    bridge's own port instead of 443 and produced a ws:// address that every
    browser blocks on an HTTPS page.

    So the installer says so instead, since it is the only party that knows: it
    sets trust_proxy when it configures nginx, and in that arrangement it also
    publishes this port on 127.0.0.1 alone. The header can then only have come
    through nginx, because nothing else can reach the socket. Without a domain
    no nginx is installed, the setting stays off, and the loopback test still
    covers the panel run directly on a host.
    """

    def __init__(self, app):
        self.app = app

    def __call__(self, environ, start_response):
        if TRUST_PROXY or environ.get("REMOTE_ADDR") in ("127.0.0.1", "::1"):
            proto = environ.get("HTTP_X_FORWARDED_PROTO")
            host  = environ.get("HTTP_X_FORWARDED_HOST")
            fwd   = environ.get("HTTP_X_FORWARDED_FOR")
            if proto in ("http", "https"):
                environ["wsgi.url_scheme"] = proto
                # remembered so the download can be handed to nginx, which only
                # works for requests that really came through it
                environ["panel.via_proxy"] = True
            if host:
                environ["HTTP_HOST"] = host.split(",")[0].strip()
            if fwd:
                environ["REMOTE_ADDR"] = fwd.split(",")[0].strip()
        return self.app(environ, start_response)


app.wsgi_app = _LocalProxyFix(app.wsgi_app)


class _SchemeAwareSession(SecureCookieSessionInterface):
    """Mark the session cookie 'secure' on HTTPS requests only.

    A fixed SESSION_COOKIE_SECURE cannot work here: switched on it would break
    login over the plain-HTTP address, switched off it would let the cookie
    travel unencrypted on the HTTPS one. Deciding per request gives each address
    the strongest setting it can actually support.
    """

    def get_cookie_secure(self, app):
        return bool(has_request_context() and request.is_secure)


app.session_interface = _SchemeAwareSession()
app.config["SESSION_COOKIE_HTTPONLY"] = True
app.config["SESSION_COOKIE_SAMESITE"] = "Lax"

def csrf_token():
    """One random token per browser session, handed to every form."""
    tok = session.get("_csrf")
    if not tok:
        tok = secrets.token_urlsafe(32)
        session["_csrf"] = tok
    return tok

@app.before_request
def csrf_protect():
    """Every POST must carry the token from the page it claims to come from."""
    if request.method != "POST":
        return
    # /crash-report is the one exception, and it has to be: it is posted by the
    # game page, which has no panel session and therefore no token to carry.
    #
    # The exemption is safe because the endpoint has nothing to forge. CSRF
    # protects an action taken with somebody ELSE's authority -- and this one
    # takes no authority at all: it is unauthenticated, it changes nothing a
    # user owns, and the worst a forged request achieves is one junk file, of
    # which the rate limit permits six per address per hour.
    if request.endpoint in ("crash_report", "api_admin_warp_me"):
        return
    sent = request.form.get("_csrf", "")
    real = session.get("_csrf", "")
    if not (real and sent and hmac.compare_digest(sent, real)):
        flash(t("csrf_bad"), "error")
        return redirect(url_for("login"))

@app.context_processor
def inject_i18n():
    _cf = client_facts()
    return {"t": t, "langs": LANGS, "curlang": lang(), "csrf_token": csrf_token(),
            "brand": BRAND, "srv": server_status(), "rates": public_rates(),
            "dlsize": human_size(_cf["size"]) if _cf["size"] else "",
            "dlsha": _cf["sha256"],
            "local_only": bool(CONF.get("local_only", False)),
            "has_accounts": accounts_exist(),
            "pp_min": PASSPHRASE_MIN,
            "max_level": MAX_LEVEL,
            # The language the GAME is in -- see the note above GAME_LANGS. The
            # front page uses it too, next to the download button, so it goes in
            # the shared context rather than into one route.
            "game_langs": GAME_LANGS,
            "game_lang": game_lang(),
            "game_lang_name": game_lang_name(),
            "lang_state": (lang_status().get("state") or ""),
            # The version, and whether a newer one is known. Both are read out
            # of memory -- update_state() never touches the network, so this
            # costs a public page render nothing at all. The notice itself is
            # shown only to the operator: a player has no use for it, and
            # announcing the exact build to everyone who visits is free
            # reconnaissance for anybody looking for a known weakness.
            "panel_version": PANEL_VERSION,
            "upd": update_state(),
            "is_admin": bool(session.get("auth") or CONF.get("local_only", False)),
            # Empty unless the operator set one. It used to be a hard-coded
            # address, which meant every server built from this project pointed
            # its players at one particular person's inbox.
            "contact": str(CONF.get("contact_email", "") or "").strip()}

@app.route("/lang/<code>")
def setlang(code):
    if code in LANGS:
        session["lang"] = code
    return redirect(request.referrer or url_for("login"))

MAX_FAIL, LOCK_SEC = 5, 900
FAILS = {}

JOB_EMOJI = {0:"⚔️",4:"⚔️",5:"🗡️",1:"🗡️",2:"🔮",6:"🔮",7:"🌀",3:"🌀",8:"🐺"}
JOB_NAME  = {0:"Warrior",4:"Warrior",5:"Ninja",1:"Ninja",2:"Sura",6:"Sura",7:"Shaman",3:"Shaman",8:"Lycan"}

GOLD_PRESETS = [("💰 1 Million", 1_000_000), ("💰 10 Million", 10_000_000),
                ("💰 100 Million", 100_000_000), ("👑 1 Billion", 1_000_000_000)]
WARP_LOC = [  # (emoji, {lang:name}, coords)
  ("🏯", {"en":"Shinsoo City","de":"Shinsoo-Stadt","tr":"Shinsoo Şehri"}, "474300 954800"),
  ("🏮", {"en":"Chunjo City","de":"Chunjo-Stadt","tr":"Chunjo Şehri"}, "65900 155600"),
  ("⛩️", {"en":"Jinno City","de":"Jinno-Stadt","tr":"Jinno Şehri"}, "963500 279700"),
  ("🏜️", {"en":"Desert","de":"Wüste","tr":"Çöl"}, "2178000 632900"),
  ("🔥", {"en":"Fireland","de":"Feuerland","tr":"Ateş Ülkesi"}, "1932800 2402700"),
]
SPEED_LOC = [
  ("🚶", {"en":"Normal (reset)","de":"Normal (zurücksetzen)","tr":"Normal (sıfırla)"}, 0),
  ("🏃", {"en":"Fast (+30%)","de":"Schnell (+30%)","tr":"Hızlı (+30%)"}, 30),
  ("💨", {"en":"Very Fast (+60%)","de":"Sehr schnell (+60%)","tr":"Çok Hızlı (+60%)"}, 60),
  ("⚡", {"en":"Light Speed (+100%)","de":"Lichtgeschwindigkeit (+100%)","tr":"Işık Hızı (+100%)"}, 100),
]
# The game master ranks, in the order the game itself grades them. The strings
# are not ours to choose: common.gmlist.mAuthority is an ENUM, and a value the
# server does not recognise is silently dropped when it reads the list, which
# would look exactly like the rank being granted and then not working. WIZARD
# exists in the server's C++ but not in the ENUM, so it cannot be offered here.
#
# LOW_WIZARD is first because it is the one to hand out: it carries the
# everyday commands and not the ones that rewrite the world.
GM_RANKS = [
  ("LOW_WIZARD",  {"en":"Helper — the everyday commands",
                   "de":"Helfer — die alltäglichen Befehle",
                   "tr":"Yardımcı — günlük komutlar"}),
  ("GOD",         {"en":"Game master — nearly everything",
                   "de":"Spielleiter — fast alles",
                   "tr":"Oyun yöneticisi — neredeyse her şey"}),
  ("HIGH_WIZARD", {"en":"High game master",
                   "de":"Oberspielleiter",
                   "tr":"Üst oyun yöneticisi"}),
  ("IMPLEMENTOR", {"en":"Owner — every command there is",
                   "de":"Betreiber — jeder existierende Befehl",
                   "tr":"Sahip — var olan her komut"}),
]
GM_RANK_SET = {r for r, _ in GM_RANKS}

# Ready-made rate settings, aimed at a quiet server where questing is the point.
# The middle one is the setting most people asking for this actually want: enough
# of a push that a quest chain carries you along, without turning the game off.
RATE_PRESETS = [  # (label key, experience, item drop, yang)
  ("rates_p_normal",   100, 100, 100),
  ("rates_p_relaxed",  300, 200, 200),
  ("rates_p_fast",    1000, 500, 500),
]

def clean_rate(raw):
    """A whole percentage between 1 and 10000, or None when it is not one."""
    try:
        v = int(str(raw).strip())
    except (TypeError, ValueError):
        return None
    return v if RATE_MIN <= v <= RATE_MAX else None

def read_rates():
    """The three percentages as they stand in the database."""
    vals = {n: 100 for n in RATE_NAMES}
    with db() as c, c.cursor() as cur:
        cur.execute("SELECT name,value FROM player.web_admin_rates")
        for row in cur.fetchall():
            if row["name"] in vals:
                try:
                    vals[row["name"]] = int(row["value"])
                except (TypeError, ValueError):
                    pass
    return vals

def rates_status():
    """The 'key=value' note apply_rates.sh leaves behind. Empty when there is none."""
    out = {}
    try:
        with open(RATES_STATUS, encoding="utf-8", errors="replace") as f:
            for line in f:
                k, sep, v = line.partition("=")
                if sep:
                    out[k.strip()] = v.strip()
    except OSError:
        pass
    return out

def write_rates_status(state):
    """Say 'it is running' right away, so reloading straight after saving is honest."""
    try:
        with open(RATES_STATUS, "w", encoding="utf-8") as f:
            f.write("state=%s\ntime=%d\n" % (state, int(time.time())))
        os.chmod(RATES_STATUS, 0o600)
    except OSError:
        pass

def gold_presets_i18n():
    return GOLD_PRESETS
def warp_presets_i18n():
    lg = lang()
    return [("%s %s" % (e, n.get(lg, n["en"])), xy) for e, n, xy in WARP_LOC]
def speed_presets_i18n():
    lg = lang()
    return [("%s %s" % (e, n.get(lg, n["en"])), spd) for e, n, spd in SPEED_LOC]
def gm_ranks_i18n():
    lg = lang()
    return [(rank, n.get(lg, n["en"])) for rank, n in GM_RANKS]
def gm_rank_label(rank):
    """The readable name of a rank, for the line that says what somebody is."""
    lg = lang()
    for r, n in GM_RANKS:
        if r == rank:
            return n.get(lg, n["en"])
    return rank

def db():
    return pymysql.connect(host=CONF.get("db_host", "127.0.0.1"), user=CONF["db_user"],
                           password=CONF["db_pass"], charset="latin1",
                           autocommit=True, cursorclass=pymysql.cursors.DictCursor)

def check_pass(p):
    h = hashlib.pbkdf2_hmac("sha256", p.encode(), CONF["salt"].encode(), 200_000)
    return hmac.compare_digest(h.hex(), CONF["pass_hash"])

def m2_hash(pw):
    """MySQL PASSWORD() style hash used by Metin2 auth: * + SHA1(SHA1(pw))"""
    return "*" + hashlib.sha1(hashlib.sha1(pw.encode()).digest()).hexdigest().upper()

def rate_limited(bucket, limit, window):
    """Very simple per-IP rate limit. Returns True if the IP should be blocked."""
    ip = request.remote_addr
    now = time.time()
    key = (bucket, ip)
    hits = [ts for ts in RATE.get(key, []) if now - ts < window]
    if len(hits) >= limit:
        RATE[key] = hits
        return True
    hits.append(now)
    RATE[key] = hits
    return False

RATE = {}

# The item index carries German names with a few English and Turkish keywords
# beside them, and more than half the entries have no keywords at all. Real
# names in other languages live inside the client's encrypted packs, which is a
# project of its own -- so instead the query is translated on the way in.
#
# Only word parts that actually occur in this index, taken from counting them,
# and only ones with an unambiguous everyday translation. It is a search box:
# a word too many costs a place in the ranking, not a wrong answer.
_DE = {
    "sword":"schwert", "dagger":"dolch", "bow":"bogen", "bell":"glocke",
    "fan":"fächer", "armor":"panzer", "armour":"panzer", "helmet":"helm",
    "shield":"schild", "shoes":"schuhe", "boots":"schuhe", "bracelet":"armband",
    "necklace":"halskette", "earring":"ohrring", "ring":"ring",
    "stone":"stein", "book":"buch", "chest":"truhe", "box":"truhe",
    "potion":"trank", "elixir":"elixier", "scroll":"rolle", "key":"schlüssel",
    "moon":"mond", "full":"voll", "half":"halb", "dragon":"drachen",
    "fire":"feuer", "ice":"eis", "earth":"erd", "wind":"wind",
    "lightning":"blitz", "dark":"dunkl", "light":"licht", "holy":"heilig",
    "red":"rot", "blue":"blau", "green":"grün", "black":"schwarz",
    "white":"weiß", "brown":"braun", "yellow":"gelb", "silver":"silber",
    "gold":"gold", "iron":"eisen", "steel":"stahl", "wood":"holz",
    "clothes":"kleidung", "shirt":"trikot", "hair":"frisur", "talisman":"talisman",
    "horse":"pferd", "wolf":"wolf", "tiger":"tiger", "spirit":"geist",
    "soul":"seele", "blood":"blut", "bone":"knochen", "skull":"schädel",
    "flower":"blume", "leaf":"blatt", "seed":"samen", "egg":"ei",
    "small":"klein", "big":"groß", "great":"groß", "old":"alt", "new":"neu",
}
def expand_query(words):
    """The words typed, plus the German ones they probably mean."""
    out = []
    for w in words:
        out.append(w)
        de = _DE.get(w)
        if de and de not in out:
            out.append(de)
    return out

ITEM_PAGE     = 40     # how many the box shows before it offers to show more
ITEM_PAGE_MAX = 400    # and how far "show more" may go

@app.route("/api/items")
def api_items():
    """Live item search for the give-item box.

    Answers {"items": [...], "more": bool}. `more' is what puts the "show
    more" line at the bottom of the list: without it the box silently stopped
    at forty and looked as though nothing else existed.

    Three ways to search, because people use all three:

      * a vnum -- "299" or "#299". The exact one first, then the ones that
        start with those digits, so "29" offers 29, 290, 291...
      * a name, in the language the panel is in.
      * a name in one of the other two. Every item's German and Turkish names
        are keywords beside the displayed one, so they find it too.

    Every word typed has to appear somewhere, rather than the old count of how
    many did: searching "Full Moon Sword" used to list Half Moon Sword too,
    because two words out of three matched and nothing insisted on the third.
    A word the index has never heard of would then find nothing at all, so if
    the strict pass comes back empty the loose one runs instead: "full moon
    sord" still offers the moon swords. One misspelt word on its own finds
    nothing either way -- there is no fuzzy matching here, and pretending
    otherwise in a comment would be worse than the gap.
    """
    # `or local_open()' is not decoration: every other admin route in this file
    # goes through login_required, which lets a local install through without a
    # session because there is no passphrase to type there. This one checked the
    # session alone, so on a local server the search answered every query with
    # an empty list -- you typed, and the box below simply never appeared.
    if not (session.get("auth") or local_open()):
        return jsonify({"items": [], "more": False})
    q = request.args.get("q", "").strip().lower()
    cat = request.args.get("cat", "all")
    try:
        limit = int(request.args.get("limit", ITEM_PAGE))
    except (TypeError, ValueError):
        limit = ITEM_PAGE
    limit = max(1, min(limit, ITEM_PAGE_MAX))

    pool = [it for it in ITEMS if cat == "all" or it["c"] == cat]

    if not q:
        return jsonify({"items": pool[:limit], "more": len(pool) > limit})

    # ---- a number, with or without the # people put in front of it ----------
    digits = q.lstrip("#").strip()
    if digits.isdigit():
        want = int(digits)
        exact  = [it for it in pool if it["v"] == want]
        prefix = [it for it in pool if it["v"] != want and str(it["v"]).startswith(digits)]
        found = exact + prefix
        return jsonify({"items": found[:limit], "more": len(found) > limit})

    # ---- a name ------------------------------------------------------------
    terms = [w for w in q.split() if w]

    def strict(it):
        """Position in the list, or None when a typed word is missing.

        The word counts as present if the index has it in the language it was
        typed in OR in German, which is what most of the index used to be and
        what many keywords still are."""
        hay = it["n"].lower() + " " + it.get("k", "")
        for w in terms:
            de = _DE.get(w)
            if w in hay or (de and de in hay):
                continue
            return None
        # Matched. Now order by how much of it was the name itself, so an exact
        # name beats one that merely contains the words, which beats an item
        # found only through its other-language keywords.
        name = it["n"].lower()
        if name == q:              return 0
        if name.startswith(q):     return 1
        if q in name:              return 2
        if all(w in name for w in terms): return 3
        return 4

    out = []
    for it in pool:
        rank = strict(it)
        if rank is not None:
            out.append((rank, len(it["n"]), it["v"], it))

    if not out:
        # Nothing matched every word. Fall back to the old behaviour: rank by
        # how many words each item does match, so a misspelling still shows the
        # neighbourhood of what was meant.
        words = [w for w in expand_query(terms) if w]
        for it in pool:
            hay = it["n"].lower() + " " + it.get("k", "")
            hits = sum(1 for w in words if w in hay)
            if hits:
                out.append((-hits, len(it["n"]), it["v"], it))

    out.sort(key=lambda r: (r[0], r[1], r[2]))
    return jsonify({"items": [r[3] for r in out[:limit]], "more": len(out) > limit})

@app.route("/api/status")
def api_status():
    """Public: the front-page badge refreshes itself from this. Server-side
    cache (30 s) makes polling harmless."""
    s = server_status()
    return jsonify({"up": s["up"], "count": s["count"],
                    "online": t("srv_online"), "playing": t("srv_playing"),
                    "offline": t("srv_offline")})

@app.route("/api/checkname")
def api_checkname():
    """Public: live 'is this username free?' for the registration form.
    Registration itself reveals taken names anyway, so this leaks nothing new."""
    if rate_limited("checkname", 30, 60):
        return jsonify({"ok": False})
    lg = request.args.get("u", "").strip()
    if not (4 <= len(lg) <= 16 and lg.isalnum()):
        return jsonify({"ok": False})
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT 1 FROM account.account WHERE login=%s", (lg,))
            return jsonify({"ok": True, "free": cur.fetchone() is None})
    except Exception:
        return jsonify({"ok": False})

FAVICON = _env_path("M2PANEL_FAVICON", os.path.join(_HERE, "favicon.png"))

@app.route("/favicon.ico")
def favicon():
    """The icon out of Metin2Release.exe, extracted once at packaging time."""
    if not os.path.exists(FAVICON):
        return ("", 404)
    resp = send_file(FAVICON, mimetype="image/png")
    resp.headers["Cache-Control"] = "public, max-age=604800"
    return resp

def local_open():
    """True when the passphrase is pointless and therefore skipped.

    A local install listens on 127.0.0.1 and nothing else: the only people who
    can reach this page are already sitting at the machine. Asking them to
    invent, store and re-type a passphrase to administer their own single-player
    server is friction with nothing on the other side of it.

    What it does give up, said plainly: any program running on that computer can
    then drive the panel. On a home PC that is the same trust you already extend
    to everything else you run there. On anything reachable by other people it
    would be indefensible -- which is why this follows the installer's own
    local_only flag rather than guessing from the bind address, where a public
    server behind nginx also looks like 127.0.0.1.
    """
    return bool(CONF.get("local_only", False))

def login_required(fn):
    @wraps(fn)
    def w(*a, **k):
        if not session.get("auth") and not local_open():
            return redirect(url_for("login"))
        return fn(*a, **k)
    return w

BASE = """
<!doctype html><html lang="{{curlang}}"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{{brand}}</title>
<link rel="icon" type="image/png" href="/favicon.ico">
<style>
:root{--gold:#e9b64b;--gold2:#f7d98c;--bg:#0e0c09;--card:#181410;--card2:#1f1a14;--line:#332b1d;
--txt:#f0eadd;--muted:#a89d84;--green:#57c15f;--red:#e05b5b;--glow:rgba(233,182,75,.16)}
*{box-sizing:border-box}
body{font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;margin:0;padding:0 0 48px;color:var(--txt);
background:radial-gradient(1100px 520px at 50% -160px,#2b2210 0%,var(--bg) 62%) fixed var(--bg)}
.top{position:sticky;top:0;z-index:20;display:flex;justify-content:space-between;align-items:center;gap:10px;
padding:14px 20px;background:rgba(14,12,9,.86);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);
border-bottom:1px solid var(--line)}
.top::after{content:"";position:absolute;left:0;right:0;bottom:-1px;height:1px;
background:linear-gradient(90deg,transparent,var(--gold),transparent);opacity:.55}
/* The gradient and the row layout live on the <a>, not on the <h1>: the title
   is a link home, and background-clip:text only paints the element that holds
   the text. Left on the h1 it would clip nothing and the words would come out
   transparent. */
.top h1{margin:0;font-size:19px;letter-spacing:.3px}
.top h1 a{display:flex;align-items:center;gap:9px;text-decoration:none;
background:linear-gradient(92deg,var(--gold),var(--gold2) 60%,var(--gold));
-webkit-background-clip:text;background-clip:text;color:transparent;
transition:filter .2s}
.top h1 a:hover{filter:brightness(1.15)}
.top h1 img{width:24px;height:24px;border-radius:5px;flex:none;
box-shadow:0 0 8px rgba(233,182,75,.35);image-rendering:auto}
.wrap{max-width:780px;margin:0 auto;padding:18px 16px}
a{color:var(--gold);text-decoration:none;transition:color .15s}
a:hover{color:var(--gold2)}
.card{background:linear-gradient(180deg,var(--card2),var(--card));border:1px solid var(--line);border-radius:16px;
padding:18px;margin-bottom:16px;box-shadow:0 10px 28px rgba(0,0,0,.4);
animation:rise .5s cubic-bezier(.22,.7,.35,1) both;transition:border-color .25s}
.wrap>.card:nth-of-type(2){animation-delay:.06s}
.wrap>.card:nth-of-type(3){animation-delay:.12s}
.wrap>.card:nth-of-type(4){animation-delay:.18s}
.wrap>.card:nth-of-type(5){animation-delay:.24s}
.card:hover{border-color:#4a3d24}
.card h3{margin:0 0 10px;font-size:17px}
/* A heading that stands between groups of cards rather than inside one, so the
   settings that belong to the installation are visibly not the ones you use
   while running the server. Full-width rule above it: on a narrow screen the
   cards are the only structure there is, and without the line a heading just
   looks like a card that lost its box. */
.sect{margin:34px 0 12px;padding-top:20px;border-top:1px solid var(--line);
font-size:15px;letter-spacing:.4px;text-transform:uppercase;color:var(--gold)}
.sect+.sect-hint{margin:-6px 0 14px;font-size:13px}
@keyframes rise{from{opacity:0;transform:translateY(14px)}to{opacity:1;transform:none}}
table{border-collapse:collapse;width:100%}
th{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.5px}
th,td{padding:10px 8px;border-bottom:1px solid var(--line);text-align:left;font-size:15px}
tr{transition:background .15s}
tr:hover td{background:var(--glow)}
input,select{background:#131007;color:var(--txt);border:1px solid var(--line);padding:12px;border-radius:12px;
font-size:16px;width:100%;margin:4px 0;transition:border-color .18s,box-shadow .18s}
input:focus,select:focus{outline:none;border-color:var(--gold);box-shadow:0 0 0 3px var(--glow)}
input::placeholder{color:#6f6650}
button,.btn{background:linear-gradient(180deg,var(--gold2),var(--gold));color:#241c0d;font-weight:700;border:none;
padding:12px 18px;border-radius:12px;font-size:16px;cursor:pointer;margin:4px 0;display:inline-block;
transition:transform .16s,box-shadow .16s,filter .16s;box-shadow:0 4px 14px rgba(0,0,0,.35)}
button:hover,.btn:hover{transform:translateY(-1px);box-shadow:0 8px 22px var(--glow),0 4px 14px rgba(0,0,0,.35);filter:saturate(1.08)}
button:active,.btn:active{transform:translateY(0) scale(.985)}
.big{width:100%;padding:16px;font-size:17px}
.flash{background:#1c3d20;border:1px solid #3d7a42;padding:12px 14px;border-radius:12px;margin-bottom:12px;
font-size:15px;animation:rise .35s ease both;word-break:break-word}
.err{background:#47201f;border-color:#8a3f3a}
.muted{color:var(--muted);font-size:13px}
.badge{font-size:13px;padding:4px 12px;border-radius:999px;background:#241d12;border:1px solid var(--line);display:inline-block}
.row{display:flex;gap:8px;flex-wrap:wrap}.row>*{flex:1;min-width:130px}
.dot{width:9px;height:9px;border-radius:50%;display:inline-block;vertical-align:1px;margin-right:7px;background:var(--red)}
.dot.on{background:var(--green);animation:pulse 2.2s ease-out infinite}
@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(87,193,95,.45)}70%{box-shadow:0 0 0 9px rgba(87,193,95,0)}100%{box-shadow:0 0 0 0 rgba(87,193,95,0)}}

/* ---- first-run prompt ---------------------------------------------------
   Shown only while the server has no account at all. The point is to answer
   "what now?" for somebody who has just installed this and is looking at a
   page full of equally plausible buttons -- so the road is drawn as three
   short steps with the current one lit, and the one action that matters is
   the only thing that moves. Everything here stops under the
   prefers-reduced-motion rule further down.                              */
.onboard{border-color:rgba(233,182,75,.45);
  box-shadow:0 10px 28px rgba(0,0,0,.4),0 0 0 1px rgba(233,182,75,.10),0 0 26px -6px var(--glow);
  animation:rise .5s cubic-bezier(.22,.7,.35,1) both,breathe 4s ease-in-out 1.2s infinite}
@keyframes breathe{0%,100%{box-shadow:0 10px 28px rgba(0,0,0,.4),0 0 0 1px rgba(233,182,75,.10),0 0 26px -6px var(--glow)}
  50%{box-shadow:0 10px 28px rgba(0,0,0,.4),0 0 0 1px rgba(233,182,75,.22),0 0 34px -4px rgba(233,182,75,.30)}}

.steps3{display:flex;align-items:flex-start;justify-content:center;gap:0;margin:14px 0 10px}
.s3{display:flex;flex-direction:column;align-items:center;gap:5px;width:84px;flex:none}
.s3 b{width:28px;height:28px;border-radius:50%;display:flex;align-items:center;justify-content:center;
  font-size:13px;font-weight:700;background:var(--card);border:1px solid var(--line);color:var(--muted);
  transition:all .3s}
.s3 i{font-style:normal;font-size:11px;line-height:1.25;color:var(--muted);transition:color .3s}
.s3.now b{background:linear-gradient(180deg,var(--gold2),var(--gold));border-color:var(--gold);color:#2a1e08;
  animation:steppulse 2s ease-out infinite}
.s3.now i{color:var(--gold2);font-weight:600}
.s3.done b{border-color:var(--green);color:var(--green)}
.s3.done b::after{content:"✓";font-size:14px}
.s3.done b{font-size:0}
@keyframes steppulse{0%{box-shadow:0 0 0 0 rgba(233,182,75,.55)}70%{box-shadow:0 0 0 10px rgba(233,182,75,0)}
  100%{box-shadow:0 0 0 0 rgba(233,182,75,0)}}
/* the connector: a track with a highlight travelling along it, so the eye is
   pulled from step 1 towards the rest rather than sitting still */
.s3bar{flex:1;height:2px;background:var(--line);margin-top:13px;border-radius:2px;
  position:relative;overflow:hidden;min-width:14px}
.s3bar u{position:absolute;inset:0;display:block;text-decoration:none;
  background:linear-gradient(90deg,transparent,var(--gold),transparent);
  transform:translateX(-100%);animation:travel 2.4s ease-in-out .6s infinite}
@keyframes travel{0%{transform:translateX(-100%)}55%,100%{transform:translateX(100%)}}

/* ---- the two ways into the game -----------------------------------------
   Framed together, and labelled, because that is the question a visitor
   actually has: not "which of these buttons is the download" but "how do I
   play". Inside the frame they are alternatives, so a rule with the word
   between the two lines separates them rather than more whitespace -- two
   cards under each other read as step one and step two.               */
.playways{max-width:460px;margin:0 auto 16px;padding:14px;border:1px solid var(--line);
  border-radius:16px;background:rgba(255,255,255,.02)}
.playways>.card{margin:0}
.playways-t{margin:0 0 12px;font-size:12px;font-weight:700;letter-spacing:1.6px;
  color:var(--muted);text-align:center}
.orsep{display:flex;align-items:center;gap:12px;margin:14px 2px;
  font-size:18px;font-weight:800;letter-spacing:3px;color:#cdc5b0}
.orsep::before,.orsep::after{content:"";flex:1;height:1px;background:var(--line)}

/* ---- the browser client, once it is ready to play ------------------------
   This card is the answer to "and now?" for somebody who has just made an
   account, so it is the one thing on the page that moves. Three layers, all
   cheap: a sheen that travels across the card, a ring that breathes, and a
   button that pulses. The global prefers-reduced-motion rule at the bottom of
   this stylesheet switches every one of them off.                         */
.playcard{position:relative;overflow:hidden;
  border-color:rgba(87,193,95,.45);
  animation:rise .5s cubic-bezier(.22,.7,.35,1) both,playbreathe 3.4s ease-in-out .8s infinite}
@keyframes playbreathe{
  0%,100%{box-shadow:0 10px 28px rgba(0,0,0,.4),0 0 0 1px rgba(87,193,95,.12),0 0 26px -6px rgba(87,193,95,.30)}
  50%    {box-shadow:0 10px 28px rgba(0,0,0,.4),0 0 0 1px rgba(87,193,95,.30),0 0 38px -2px rgba(87,193,95,.45)}}
/* the sheen: a wide, very faint diagonal band that crosses every few seconds */
.playcard::after{content:"";position:absolute;top:-60%;left:-60%;width:60%;height:220%;pointer-events:none;
  background:linear-gradient(100deg,transparent,rgba(255,255,255,.13),transparent);
  transform:translateX(-60%) rotate(12deg);animation:sheen 4.5s ease-in-out 1.4s infinite}
@keyframes sheen{0%{transform:translateX(-60%) rotate(12deg)}
  45%,100%{transform:translateX(420%) rotate(12deg)}}
.playcard h3{animation:none}
/* the button: a green pulse rather than the panel's gold one, because this is
   "ready, go" and not "look here" -- and a small lift so it reads as pressable */
.btn.play{animation:playpulse 2.1s ease-out infinite;transition:transform .15s ease}
.btn.play:hover{transform:translateY(-2px)}
@keyframes playpulse{
  0%  {box-shadow:0 0 0 0 rgba(87,193,95,.50),0 2px 10px rgba(0,0,0,.35)}
  70% {box-shadow:0 0 0 12px rgba(87,193,95,0),0 2px 10px rgba(0,0,0,.35)}
  100%{box-shadow:0 0 0 0 rgba(87,193,95,0),0 2px 10px rgba(0,0,0,.35)}}
.btn.glow{animation:btnglow 2.6s ease-in-out infinite}
@keyframes btnglow{0%,100%{box-shadow:0 2px 10px rgba(233,182,75,.20)}
  50%{box-shadow:0 4px 20px rgba(233,182,75,.42)}}
.steps{margin:10px 0 4px;padding:0;list-style:none;counter-reset:s;text-align:left}
.steps li{counter-increment:s;margin:9px 0;padding-left:36px;position:relative;font-size:14px;color:#d8d0bd}
.steps li::before{content:counter(s);position:absolute;left:0;top:-2px;width:24px;height:24px;border-radius:50%;
background:var(--glow);border:1px solid var(--gold);color:var(--gold);font-weight:700;font-size:13px;
display:flex;align-items:center;justify-content:center}
.hintline{font-size:13px;min-height:18px;margin:2px 0 6px}
.ok-t{color:var(--green)}.bad-t{color:var(--red)}
/* Anything carrying an explanation says so quietly: the cursor changes, and
   text you can hover gets a faint dotted underline. No popups, no scripting -
   the browser's own tooltip does the work. */
[title]{cursor:help}
button[title],.btn[title],a.btn[title],select[title]{cursor:pointer}
input[title]{cursor:text}
.help{border-bottom:1px dotted #6b6350}
.about p{margin:0 0 10px;line-height:1.6;font-size:14px;color:#cdc5b0}
.about p:last-child{margin-bottom:0}
/* ---- the rendered changelog --------------------------------------------
   Everything under .md was produced by md_to_html(): a fixed set of tags,
   built here, out of text that was HTML-escaped before a single one of them
   was added. It is styled as a document rather than as part of the panel,
   because that is what it is. */
.md{font-size:14px;line-height:1.65;color:#cdc5b0}
.md h2{font-size:17px;margin:20px 0 8px;color:var(--gold2)}
.md h2:first-child{margin-top:0}
.md h3{font-size:15px;margin:16px 0 6px;color:var(--txt)}
.md h4,.md h5{font-size:13px;margin:14px 0 6px;color:var(--muted);text-transform:uppercase;letter-spacing:.5px}
.md p{margin:0 0 10px}
.md ul,.md ol{margin:0 0 10px;padding-left:22px}
.md li{margin:5px 0}
.md hr{border:none;border-top:1px solid var(--line);margin:18px 0}
.md code,.cmd{background:#131007;border:1px solid var(--line);border-radius:6px;padding:1px 5px;
font-family:ui-monospace,'Cascadia Mono',Consolas,monospace;font-size:13px}
.md pre,pre.cmd{background:#131007;border:1px solid var(--line);border-radius:10px;padding:12px;
overflow-x:auto;white-space:pre;margin:8px 0}
.md pre code,pre.cmd{border:none;background:none;padding:0}
pre.cmd{padding:12px;border:1px solid var(--line);background:#131007;color:#cdc5b0;line-height:1.5}
.md strong{color:var(--txt)}
.md a{text-decoration:underline}
@media (prefers-reduced-motion:reduce){*,*::before,*::after{animation:none!important;transition:none!important}}
</style></head><body>
{# The title is the way home, as it is on every other site. Worth having even
   where a logout link exists: from a player page it is one click instead of
   two, and on a local install it is the only route back. #}
<div class="top"><h1 title="{{t('about_goal')}}"><a href="{{url_for('login')}}"><img src="/favicon.ico" alt="">{{brand}}</a> <a href="{{url_for('live_map')}}" style="font-size:14px;margin-left:14px;color:#e9b64b;text-decoration:none;font-weight:700;padding:3px 10px;background:rgba(233,182,75,0.12);border:1px solid rgba(233,182,75,0.3);border-radius:6px">🗺️ {{'Mapa na żywo' if curlang == 'pl' else 'Live map'}}</a></h1>
<div>
<span style="font-size:13px" title="{{t('tip_lang')}}">
{% for code, name in langs.items() %}<a href="{{url_for('setlang', code=code)}}" title="{{name}}" style="margin:0 3px;{{'font-weight:700;text-decoration:underline' if code==curlang else 'opacity:.7'}}">{{code|upper}}</a>{% endfor %}
</span>
{% if session.get('auth') %}&nbsp;<a href="{{url_for('logout')}}" title="{{t('tip_logout')}}">{{t('logout')}} 🚪</a>{% endif %}
{# A local install never logs in, so it never gets a logout link either -- and
   without one there was no way back to the front page from the admin side. #}
{% if local_only and request.endpoint != 'login' %}&nbsp;<a href="{{url_for('login')}}">{{t('back_front')}}</a>{% endif %}</div></div>
<div class="wrap">
{% with m = get_flashed_messages(with_categories=true) %}{% for c,msg in m %}
<div class="flash {{'err' if c=='error' else ''}}">{{msg}}</div>{% endfor %}{% endwith %}
__BODY__
</div>
{# Which build this is -- small, at the bottom, and only for the operator.
   Findable: it is on every admin page and it is the way into the patch log.
   Unobtrusive: it is one grey line. Not public: a player has no use for the
   number and an attacker has a very specific one. #}
{# The number itself is harmless and useful -- it is the first thing anybody
   asks for when something is wrong. The changelog and the update notice are
   not: they belong to whoever runs the server, so they only appear away from
   the front page, where only the operator goes. #}
<div class="wrap" style="padding-top:0;text-align:center">
<span class="muted" style="font-size:12px">
{% if is_admin and request.endpoint != 'login' %}
<a href="{{url_for('patchlog')}}" title="{{t('tip_patchlog')}}" style="color:inherit">{{t('ver_label')}} {{ panel_version if panel_version else t('ver_unknown') }}</a>
{% if upd.available %} · <a href="{{url_for('patchlog')}}" title="{{t('tip_patchlog')}}">⬆️ {{t('upd_avail_short')}}</a>{% endif %}
{% else %}
{{t('ver_label')}} {{ panel_version if panel_version else t('ver_unknown') }}
{% endif %}
</span></div>
<p class="muted" style="text-align:center;margin:26px 0 10px;font-size:12.5px">
<a href="{{ DISCORD }}" target="_blank" rel="noopener noreferrer"
   style="color:#7a86d6;text-decoration:none">💬 {{ t('dc_foot') }}</a></p>
</body></html>"""

TPL_LOGIN = BASE.replace("__BODY__", """
<div style="max-width:560px;margin:18px auto 0;text-align:center">
<span class="badge help" id="srvbadge" title="{{t('tip_srv')}}" style="font-size:14px;padding:7px 15px">
{% if srv.up %}<span class="dot on"></span>{{t('srv_online')}} — <b>{{srv.count}}</b> {{t('srv_playing')}}
{% else %}<span class="dot"></span>{{t('srv_offline')}}{% endif %}</span>
{% if rates %}
<div style="margin-top:9px">
<span class="badge help" title="{{t('tip_rates')}}">⭐ {{t('rates_exp')}} {{rates['exp']}}%</span>
<span class="badge help" title="{{t('tip_rates')}}">🎁 {{t('rates_drop')}} {{rates['drop']}}%</span>
<span class="badge help" title="{{t('tip_rates')}}">💰 {{t('rates_yang')}} {{rates['yang']}}%</span>
</div>
{% endif %}
</div>
<script>
setInterval(function(){
 fetch('/api/status').then(function(r){return r.json();}).then(function(s){
  var b=document.getElementById('srvbadge'); if(!b)return;
  b.innerHTML = s.up ? '<span class="dot on"></span>'+s.online+' — <b>'+s.count+'</b> '+s.playing
                     : '<span class="dot"></span>'+s.offline;
 }).catch(function(){});
},60000);
</script>
<div class="card about" style="max-width:560px;margin:24px auto">
<h3>ℹ️ {{t('about_title')}}</h3>
<p>{{t('about_goal')}}</p>
<p>{{t('about_hobby')}}</p>
{% if browser_ready %}<p>{{t('about_web')}}</p>{% endif %}
<p>{{t('about_uptime')}}</p>
<p>{{t('about_oss')}}</p>
{% if contact %}<p>{{t('about_contact')}} <a href="mailto:{{contact}}">{{contact}}</a>.</p>{% endif %}
</div>
<div class="card{% if not has_accounts %} onboard{% endif %}" style="max-width:380px;margin:0 auto 16px;text-align:center">
<div style="font-size:40px">🧑‍🤝‍🧑</div>
<h3>{% if has_accounts %}{{t('game_account')}}{% else %}{{t('ob_title')}}{% endif %}</h3>
{% if not has_accounts %}
{# Nobody has registered yet, so the page says so instead of showing two
   equal-weight buttons and leaving a first-time visitor to guess. The three
   steps are there to show how short the road is, not to decorate. #}
<div class="steps3" aria-hidden="true">
  <span class="s3 now"><b>1</b><i>{{t('ob_s1')}}</i></span>
  <span class="s3bar"><u></u></span>
  <span class="s3 {% if client_ready or local_only %}done{% endif %}"><b>2</b><i>{{t('ob_s2')}}</i></span>
  <span class="s3bar"></span>
  <span class="s3"><b>3</b><i>{{t('ob_s3')}}</i></span>
</div>
<p class="muted" style="margin:2px 0 12px">{{t('ob_none')}}</p>
<a class="btn big glow" href="{{url_for('register')}}" title="{{t('tip_create_acc')}}">✨ {{t('ob_go')}}</a>
{% else %}
<p class="muted" style="margin:2px 0 12px">{{t('acc_needed')}}</p>
<div class="row">
<a class="btn" href="{{url_for('register')}}" title="{{t('tip_create_acc')}}">{{t('create_acc')}}</a>
<a class="btn" href="{{url_for('account')}}" title="{{t('tip_my_acc')}}">{{t('my_acc')}}</a>
</div>
{% endif %}
</div>
{% set has_desktop = local_only or client_ready or client_url %}
{% if browser_ready or has_desktop %}
{# Both ways in, inside one frame and under one heading. Whichever of the two
   this server has is in here; when it has both, the rule with ODER on it says
   they are alternatives. #}
<section class="playways">
<div class="playways-t">{{t('ways_t')}}</div>
{% endif %}
{% if browser_ready %}
{# First, because it is the shorter road for somebody who just wants to look:
   nothing to fetch and nothing to install. #}
<div class="card playcard" style="max-width:420px;margin:0 auto;text-align:center">
<div style="font-size:40px">🌐</div>
<h3>{{t('play_title')}}</h3>
<p class="muted">{{t('play_hint')}}</p>
<a class="btn big play" href="{{play_url}}" target="_blank" rel="noopener"
   title="{{t('tip_play')}}">{{t('play_btn')}}</a>
</div>
{% endif %}
{% if browser_ready and has_desktop %}<div class="orsep">{{t('reg_or')}}</div>{% endif %}
{% if local_only %}
{# A local server plays on the machine it runs on, so there is nothing to
   fetch over the network. Point at the Desktop shortcut instead of at a
   download button that would only copy a file to where it already is. #}
<div class="card" style="max-width:420px;margin:0 auto;text-align:center">
<div style="font-size:40px">🎮</div>
<h3>{{t('dl_local_t')}}</h3>
<p class="muted">{% if client_ready %}{{t('dl_local')|safe}}{% else %}{{t('dl_local_w')}}{% endif %}</p>
</div>
{% elif client_ready or client_url %}
<div class="card" style="max-width:420px;margin:0 auto;text-align:center">
<div style="font-size:40px">📥</div>
<h3>{{t('dl_now_t')}}</h3>
<p class="muted">{{t('dl_hint')}}</p>
<a class="btn big" href="{{ client_url if client_url else url_for('download') }}"
   title="{{t('tip_download')}}"
   {% if client_url %}rel="noopener noreferrer"{% endif %}>{% if client_name %}📥 {{client_name}}{% else %}{{t('download')}}{% endif %}{% if dlsize %} <span style="font-weight:400;font-size:13px">({{dlsize}})</span>{% endif %}</a>
{# Which language the game itself is in. Worth a line of its own on the page a
   player arrives at: it is the one thing about this server they cannot see
   until they have downloaded a gigabyte and started it. #}
<p class="muted" style="font-size:13px;margin:6px 0 0">🌍 {{t('dl_lang').format(lang=game_lang_name)}}</p>
<ol class="steps">
<li>{{t('dl_st1')}}{% if dlsize %} ({{dlsize}}){% endif %}</li>
<li>{{t('dl_st2')}}</li>
<li>{{t('dl_st3')}}</li>
</ol>
{% if dlsha and not client_url %}<p class="muted help" title="{{t('dl_sha')}}" style="font-size:11px;word-break:break-all">SHA-256: {{dlsha}}</p>{% endif %}
</div>
{% endif %}
{% if browser_ready or has_desktop %}</section>{% endif %}
<div class="card" style="max-width:380px;margin:0 auto;text-align:center">
<div style="font-size:40px">{% if local_only %}🛠️{% else %}🔑{% endif %}</div>
<h3>{{t('welcome')}}</h3>
{% if local_only %}
<p class="muted">{{t('admin_hint_local')}}</p>
<a class="btn big" href="/admin">{{t('admin_open')}}</a>
{% else %}
<p class="muted">{{t('admin_hint')}}</p>
<form method="post"><input type="hidden" name="_csrf" value="{{csrf_token}}">
<input type="password" name="pw" placeholder="{{t('passphrase')}}" title="{{t('tip_passphrase')}}">
<button class="big" title="{{t('tip_login')}}">{{t('login')}}</button></form>
{% endif %}
</div>""")

TPL_DL_LIMIT = BASE.replace("__BODY__", """
<div class="card" style="max-width:420px;margin:40px auto;text-align:center">
<div style="font-size:48px">⏳</div>
<h3>{{t('dl_limit_title')}}</h3>
<p class="muted" style="font-size:15px">{{ t('dl_limit_all' if scope == 'all' else 'dl_limit').replace('{h}', wait_h|string) }}</p>
<p><a href="{{url_for('login')}}">← Back</a></p></div>""")

TPL_RESET = BASE.replace("__BODY__", """
<div class="card" style="max-width:420px;margin:40px auto;text-align:center">
<div style="font-size:48px">🔑</div>
<h3>{{t('reset_set_title')}}</h3>
{% if valid %}
<p class="muted">{{t('reset_for')}} <b>{{login}}</b></p>
<form method="post"><input type="hidden" name="_csrf" value="{{csrf_token}}">
<input type="password" name="new" placeholder="{{t('reset_ph1')}}" autofocus>
<input type="password" name="new2" placeholder="{{t('reset_ph2')}}">
<button class="big">{{t('reset_set_btn')}}</button></form>
{% else %}
<p class="muted">{{t('reset_bad_link')}}</p>
<p><a href="{{url_for('login')}}">← Back</a></p>
{% endif %}
</div>""")

TPL_REGISTER = BASE.replace("__BODY__", """
<p><a href="{{url_for('login')}}">← Back</a></p>
<div class="card" style="max-width:440px;margin:20px auto">
<h3>📝 {{t('reg_title')}}</h3>
<p class="muted">{{t('reg_hint')}}</p>
<form method="post">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input name="login" id="regName" placeholder="{{t('reg_ph_user')}}" value="{{form.login}}" title="{{t('tip_create_acc')}}" autocomplete="username" autofocus>
<div class="hintline" id="nameHint"></div>
<input type="password" name="pw" id="regPw" placeholder="{{t('reg_ph_pw')}}" autocomplete="new-password">
<input type="password" name="pw2" id="regPw2" placeholder="{{t('reg_ph_pw2')}}" autocomplete="new-password">
<div class="hintline" id="pwHint"></div>
<input name="social" placeholder="{{t('reg_ph_social')}}" value="{{form.social}}" title="{{t('tip_delcode')}}" inputmode="numeric">
<p class="muted">{{t('reg_social_hint')}}</p>
<button class="big">{{t('create_acc')}}</button>
</form></div>
<script>
(function(){
 var n=document.getElementById('regName'),nh=document.getElementById('nameHint'),
     p1=document.getElementById('regPw'),p2=document.getElementById('regPw2'),
     ph=document.getElementById('pwHint'),tmr=null;
 var TXT={free:{{t('reg_free')|tojson}},taken:{{t('reg_taken')|tojson}},
          match:{{t('reg_pw_match')|tojson}},diff:{{t('reg_pw_diff')|tojson}}};
 n.addEventListener('input',function(){
   clearTimeout(tmr); nh.textContent=''; nh.className='hintline';
   var v=n.value.trim();
   if(!/^[A-Za-z0-9]{4,16}$/.test(v)) return;
   tmr=setTimeout(function(){
     fetch('/api/checkname?u='+encodeURIComponent(v)).then(function(r){return r.json();}).then(function(d){
       if(!d.ok || n.value.trim()!==v) return;
       nh.textContent = d.free?TXT.free:TXT.taken;
       nh.className = 'hintline '+(d.free?'ok-t':'bad-t');
     }).catch(function(){});
   },350);
 });
 function pwc(){
   if(!p2.value){ph.textContent='';ph.className='hintline';return;}
   var same = p1.value===p2.value;
   ph.textContent = same?TXT.match:TXT.diff;
   ph.className = 'hintline '+(same?'ok-t':'bad-t');
 }
 p1.addEventListener('input',pwc); p2.addEventListener('input',pwc);
})();
</script>""")

# What a new player is told to do next depends on what this server actually
# offers, and the three cases are genuinely different pages:
#
#   desktop only   the original: download, unpack, run.
#   browser only   there is nothing to download, and telling somebody to unpack
#                  a zip that does not exist is worse than saying nothing.
#   both           two cards, side by side, with ODER between them -- because
#                  they are alternatives, not steps. The account is the same in
#                  either, and the page says so: without that line two cards
#                  read as two different games.
#
# The cards are flex items with a min-width, so a phone stacks them without a
# media query and the OR falls between the two on its own line.
TPL_REG_DONE = BASE.replace("__BODY__", """
{% macro web_card() %}
<div class="card playcard" style="flex:1 1 300px;max-width:380px;margin:0;text-align:center">
<div style="font-size:40px">🌐</div>
<h3>{{t('play_title')}}</h3>
<p class="muted">{{t('play_hint')}}</p>
<ol class="steps">
<li>{{t('reg_done_login')}}</li>
</ol>
<a class="btn big play" href="{{play_url}}" target="_blank" rel="noopener"
   title="{{t('tip_play')}}">{{t('play_btn')}}</a>
</div>
{% endmacro %}

{% macro desktop_card() %}
<div class="card" style="flex:1 1 300px;max-width:380px;margin:0;text-align:center">
<div style="font-size:40px">📥</div>
<h3>{{t('reg_dl_t')}}</h3>
<ol class="steps">
<li>{{t('dl_st1')}}{% if dlsize %} ({{dlsize}}){% endif %}</li>
<li>{{t('dl_st2')}}</li>
<li>{{t('dl_st3')}}</li>
<li>{{t('reg_done_login')}}</li>
</ol>
{% if local_only %}
<p class="muted">{{t('dl_local')|safe}}</p>
{% else %}
<a class="btn big" href="{{ client_url if client_url else url_for('download') }}"
   title="{{t('tip_download')}}" {% if client_url %}rel="noopener noreferrer"{% endif %}>{{t('download')}}</a>
<p class="muted" style="font-size:13px;margin:6px 0 0">🌍 {{t('dl_lang').format(lang=game_lang_name)}}</p>
{% endif %}
</div>
{% endmacro %}

{% set has_desktop = local_only or client_ready or client_url %}
{% set both = browser_ready and has_desktop %}

<div style="max-width:{{ '860px' if both else '440px' }};margin:40px auto;text-align:center">
<div style="font-size:48px">🎉</div>
<h3>{{t('reg_done_title')}}</h3>
<p class="muted">{{ t('reg_both') if both else t('reg_done_next') }}</p>

<div style="display:flex;flex-wrap:wrap;align-items:center;justify-content:center;gap:18px;margin-top:18px">
{% if browser_ready %}{{ web_card() }}{% endif %}
{% if both %}
<div style="font-size:26px;font-weight:800;letter-spacing:2px;flex:0 0 auto">{{t('reg_or')}}</div>
{% endif %}
{% if has_desktop %}{{ desktop_card() }}{% endif %}
</div>

{% if not browser_ready and not has_desktop %}
{# Neither is set up. The account is still real and still works, so say that
   much rather than showing an empty page under a title that says it is ready. #}
<ol class="steps" style="display:inline-block;text-align:left"><li>{{t('reg_done_login')}}</li></ol>
{% endif %}

<p style="margin-top:16px"><a href="{{url_for('login')}}">← {{brand}}</a></p></div>""")

TPL_ACCOUNT_LOGIN = BASE.replace("__BODY__", """
<p><a href="{{url_for('login')}}">← Back</a></p>
<div class="card" style="max-width:420px;margin:20px auto;text-align:center">
<div style="font-size:40px">👤</div>
<h3>My account</h3>
<p class="muted">Log in with your game username and password.</p>
<form method="post">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input name="login" placeholder="Username" autofocus>
<input type="password" name="pw" placeholder="Password">
<button class="big">Log in</button>
</form></div>""")

TPL_ACCOUNT = BASE.replace("__BODY__", """
<p><a href="{{url_for('login')}}">← Home</a> &nbsp;|&nbsp; <a href="{{url_for('account_logout')}}">Log out of account</a></p>
<div class="card" style="border-left:3px solid #5865F2">
<h3 style="margin-top:0">{{ t('dc_title') }}</h3>
<p class="muted" style="margin-bottom:14px">{{ t('dc_body') }}</p>
<a class="btn" href="{{ DISCORD }}" target="_blank" rel="noopener noreferrer">{{ t('dc_btn') }}</a>
</div>
<div class="card">
<h3>👤 {{login}}</h3>
<p class="muted">Your characters:</p>
<table>
<tr><th>Character</th><th>Level</th><th>Yang</th></tr>
{% for ch in chars %}
<tr><td>{{emoji(ch.job)}} <b>{{ch.name}}</b></td><td>{{ch.level}}</td><td>{{"{:,}".format(ch.gold)}}</td></tr>
{% endfor %}
</table>
{% if not chars %}<p>No characters yet — log into the game and create one! 🙂</p>{% endif %}
</div>
<div class="card"><h3>🔒 Change password</h3>
<form method="post" action="{{url_for('account_password')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input type="password" name="old" placeholder="Current password">
<input type="password" name="new" placeholder="New password (at least 6 characters)">
<input type="password" name="new2" placeholder="New password again">
<button class="big">🔒 Change password</button>
</form></div>""")

TPL_DASH = BASE.replace("__BODY__", """
<a href="#" id="introshow" style="display:none;font-size:13px" class="muted">{{t('op_show')}}</a>
<div class="card about" id="intro">
<h3>✅ {{t('op_title')}}</h3>
<p>{{t('op_intro')}}</p>
{% if local_only %}
<p><b>🔒 {{t('op_local_t')}}.</b> {{t('op_local')}}</p>
<p>{{t('op_local_hint')}}</p>
{% else %}
<p>{{t('op_share')}}</p>
{% endif %}
<p>{{t('op_rates')}}</p>
<p>{{t('op_players')}}</p>
<p>{{t('op_limits')}}</p>
<p>{{t('op_forgot')}}</p>
<p>{{t('op_pp')}}</p>
<p class="muted">{{t('op_more')}}</p>
<button class="btn" id="introhide" type="button">{{t('op_hide')}}</button>
</div>
<script>
(function(){
 var box=document.getElementById('intro'), hide=document.getElementById('introhide'),
     show=document.getElementById('introshow'), KEY='m2_intro_hidden';
 if(!box||!hide||!show) return;
 function apply(h){ box.style.display = h ? 'none' : ''; show.style.display = h ? '' : 'none'; }
 var stored=false;
 try { stored = localStorage.getItem(KEY)==='1'; } catch(e){}   // private mode: just show it
 apply(stored);
 hide.addEventListener('click',function(){ try{localStorage.setItem(KEY,'1');}catch(e){} apply(true); });
 show.addEventListener('click',function(e){ e.preventDefault(); try{localStorage.removeItem(KEY);}catch(e){} apply(false); });
})();
</script>

{# Always here, so the changelog is one click away rather than a grey line at
   the bottom of the page. It lights up by itself when there is something to
   fetch. Only on the admin side -- the front page belongs to the players. #}
<div class="card{% if upd.available %} onboard{% endif %}">
<h3>{% if upd.available %}⬆️ {{t('upd_avail_t')}}{% else %}📜 {{t('pl_card_t')}}{% endif %}</h3>
{% if upd.available %}
<p class="muted">{{ t('upd_avail').replace('{cur}', upd.current or t('ver_unknown')).replace('{new}', upd.latest) }}</p>
<a class="btn big glow" href="{{url_for('patchlog')}}" title="{{t('tip_patchlog')}}">{{t('upd_see')}}</a>
{% else %}
<p class="muted">{{t('ver_label')}} <b>{{ panel_version if panel_version else t('ver_unknown') }}</b>.
{% if not upd.enabled %}{{t('upd_off_t')}}.{% elif upd.error %}{{t('upd_failed')}}{% elif not upd.checked %}{{t('upd_never')}}{% else %}{{t('upd_none')}}{% endif %}</p>
<a class="btn" href="{{url_for('patchlog')}}" title="{{t('tip_patchlog')}}">{{t('pl_open')}}</a>
{% endif %}
</div>
<div class="card">
<h3 class="help" title="{{t('tip_rates')}}">{{t('rates_nav')}}</h3>
<p class="muted">{{t('rates_dash_hint')}}</p>
<a class="btn" href="{{url_for('rates')}}" title="{{t('tip_rates')}}">{{t('rates_open')}}</a>
</div>
<div class="card">
<h3 class="help" title="{{t('tip_reset')}}">🔗 {{t('reset_title')}}</h3>
<p class="muted">{{t('reset_hint')}}</p>
<form method="post" action="{{url_for('admin_resetlink')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input name="login" placeholder="{{t('reset_user_ph')}}" title="{{t('tip_reset')}}">
<button title="{{t('tip_reset')}}">{{t('reset_make')}}</button></form>
</div>
<div class="card">
<h3 class="help" title="{{t('tip_players')}}">👥 {{t('players')}}</h3>
<p class="muted">{{t('tap_hint')}}</p>
{% if players %}<input id="pfilter" placeholder="{{t('search_players')}}" autocomplete="off" style="margin-bottom:8px">{% endif %}
<table id="ptable">
<tr><th>{{t('character')}}</th><th class="help" title="{{t('tip_acc_col')}}">{{t('acc_col')}}</th><th>{{t('level')}}</th><th>Yang</th><th>{{t('last_seen')}}</th></tr>
{% for p in players %}
<tr data-k="{{ (p.name ~ ' ' ~ (p.account or ''))|lower }}">
<td><a href="{{url_for('player', pid=p.id)}}" title="{{t('tip_player')}}">{% if p.active %}<span class="dot on" title="{{t('tip_active')}}"></span>{% endif %}{{emoji(p.job)}} <b>{{p.name}}</b></a>
<div class="muted">{{jobname(p.job)}}</div></td>
<td title="{{t('tip_acc_col')}}">👤 {{p.account or '—'}}</td>
<td>{{p.level}}</td><td>{{"{:,}".format(p.gold)}}</td>
<td class="muted">{{p.last_play}}</td></tr>
{% endfor %}</table>
{% if not players %}<p>{{t('no_chars')}} 🙂</p>{% endif %}
</div>
<script>
(function(){
 var f=document.getElementById('pfilter'); if(!f) return;
 var rows=document.querySelectorAll('#ptable tr[data-k]');
 f.addEventListener('input',function(){
   var q=f.value.toLowerCase().trim();
   rows.forEach(function(r){ r.style.display = r.getAttribute('data-k').indexOf(q)>=0 ? '' : 'none'; });
 });
})();
</script>

{# Everything above is the server as you run it day to day: what is published,
   how fast it plays, who is on it. What follows is the installation itself --
   set once, rarely touched, and disruptive when it is. Keeping the two apart
   is why a language switch that restarts the game does not sit next to the
   list of characters you click through every day. #}
<h2 class="sect">🛠️ {{t('sect_setup')}}</h2>
<p class="muted sect-hint">{{t('sect_setup_hint')}}</p>

<div class="card"><h3 class="help" title="{{t('tip_gamelang')}}">{{t('lang_title')}}</h3>
<p><b>{{t('lang_now').format(lang=game_lang_name)}}</b></p>
<p class="muted">{{t('lang_intro')}}</p>
{% if lang_state == 'running' %}<p class="muted">⏳ {{t('lang_busy')}}</p>
{% elif lang_state == 'failed' %}<p class="muted">⚠️ {{t('lang_failed')}}</p>
{% elif lang_state == 'unsupported' %}<p class="muted">⚠️ {{t('lang_unsup')}}</p>{% endif %}
<form method="post" action="{{url_for('set_language')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<select name="lang" title="{{t('tip_gamelang')}}">
{% for code, name in game_langs %}<option value="{{code}}"{% if code == game_lang %} selected{% endif %}>{{name}}</option>{% endfor %}
</select>
<button class="big" title="{{t('tip_gamelang')}}">{{t('lang_apply')}}</button></form>

{# The other half of a language change, and the half the panel cannot do: the
   client's menus and item names come from a pack chosen by a file sitting next
   to the .exe. Every language is already in that folder, so it is a rename and
   not a download -- but nobody guesses that, so it is spelled out. Shown only
   when it is actually needed, i.e. when the game is not in English. #}
{% if game_lang != 'en' %}
<h3 style="margin-top:18px">{{t('lang_cl_t')}}</h3>
<p class="muted">{{t('lang_cl').format(lang=game_lang_name)}}</p>
<ol class="muted" style="margin:6px 0 0 18px">
<li>{{t('lang_cl_1')}}</li>
<li>{{t('lang_cl_2').format(file='locale_' + game_lang + '.cfg')}}</li>
</ol>
<p class="muted">{{t('lang_cl_3')}}</p>
{% endif %}
</div>

{# Its own card rather than a paragraph inside the introduction above: that one
   can be hidden for good with one click, and a way back into your own panel
   should not disappear with it. #}
<div class="card"><h3 class="help" title="{{t('tip_pp')}}">{{t('pp_title')}}</h3>
{% if local_only %}<p class="muted">{{t('pp_local')}}</p>{% endif %}
<form method="post" action="{{url_for('set_passphrase')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
{% if not local_only %}<input type="password" name="old" placeholder="{{t('pp_old')}}" title="{{t('tip_pp')}}">{% endif %}
<input type="password" name="new" placeholder="{{t('pp_new').format(n=pp_min)}}" title="{{t('tip_pp')}}">
<input type="password" name="new2" placeholder="{{t('pp_new2')}}" title="{{t('tip_pp')}}">
<button class="big" title="{{t('tip_pp')}}">{{t('pp_save')}}</button></form></div>
""")

TPL_PLAYER = BASE.replace("__BODY__", """
<p><a href="{{url_for('dash')}}">{{t('back_players')}}</a></p>
<div class="card">
<h3>{{emoji(p.job)}} {{p.name}}</h3>
<span class="badge">{{t('level')}} {{p.level}}</span>
<span class="badge">💰 {{"{:,}".format(p.gold)}} yang</span>
<span class="badge">🗺️ Map {{p.map_index}}</span>
</div>

<div class="card"><h3 class="help" title="{{t('tip_send_item')}}">{{t('give_item')}}</h3>
<form method="post" action="{{url_for('action')}}" id="itemForm">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input type="hidden" name="pid" value="{{p.id}}"><input type="hidden" name="cmd" value="ITEM">
<input type="hidden" name="custom_vnum" id="itemVnum">
<select id="itemCat" title="{{t('tip_category')}}">
{% for c in cats %}<option value="{{c}}">{{t('cat_'+c)}}</option>{% endfor %}
</select>
<input id="itemSearch" placeholder="{{t('search_item')}}" title="{{t('tip_search_item')}}" autocomplete="off">
<div id="itemResults" style="max-height:220px;overflow:auto;margin:4px 0"></div>
<div id="itemChosen" class="muted" style="margin:4px 0">—</div>
<input name="arg2" type="number" min="1" max="65535" placeholder="{{t('qty')}}" title="{{t('tip_qty')}}" value="1">
<button class="big" title="{{t('tip_send_item')}}">{{t('send_item')}}</button></form></div>
<script>
(function(){
 var s=document.getElementById('itemSearch'),c=document.getElementById('itemCat'),
     r=document.getElementById('itemResults'),v=document.getElementById('itemVnum'),
     ch=document.getElementById('itemChosen'),tmr=null;
 var shown=40;
 function load(n){
   shown=n||40;
   var q=encodeURIComponent(s.value),cat=c.value;
   /* credentials: the session cookie has to go with this or the server answers
      an empty list and the box stays blank. Modern browsers send it on a
      same-origin fetch by default -- but that default was 'omit' until 2017,
      and some browsers still drop cookies on background requests under their
      privacy settings. Saying it costs nothing and removes a whole class of
      "it works here but not there". */
   fetch('/api/items?q='+q+'&cat='+cat+'&limit='+shown,{credentials:'same-origin'})
    .then(function(x){ if(!x.ok) throw new Error(x.status); return x.json(); })
    .then(function(res){
     r.innerHTML='';
     res.items.forEach(function(it){
       var b=document.createElement('div');
       b.style.cssText='padding:8px 10px;border-bottom:1px solid #3a3222;cursor:pointer';
       b.textContent=it.n+'  ·  #'+it.v;
       b.onclick=function(){v.value=it.v;ch.innerHTML='✅ '+it.n+' (#'+it.v+')';r.innerHTML='';s.value=it.n;};
       r.appendChild(b);
     });
     /* Only when there really is more. A "show more" that shows nothing is
        worse than no button, so this follows the server's own count. */
     if(res.more){
       var m=document.createElement('div');
       m.style.cssText='padding:8px 10px;cursor:pointer;text-align:center;font-weight:600';
       m.textContent={{t('srch_more')|tojson}};
       m.onclick=function(){load(shown+120);};
       r.appendChild(m);
     }
   })
    /* Without this a failed request left the box empty and silent, which is
       indistinguishable from "no such item" and sends people looking in the
       wrong place. Now it says so on the page instead of only in a console
       nobody has open. */
    .catch(function(){
      r.innerHTML='';
      var e=document.createElement('div');
      e.style.cssText='padding:8px 10px;opacity:.75';
      e.textContent={{t('srch_down')|tojson}};
      r.appendChild(e);
    });
 }
 s.addEventListener('input',function(){clearTimeout(tmr);tmr=setTimeout(function(){load(40);},200);});
 c.addEventListener('change',function(){load(40);});
 document.getElementById('itemForm').addEventListener('submit',function(e){
   if(!v.value){e.preventDefault();ch.innerHTML='⚠️ '+s.getAttribute('placeholder');}
 });
})();
</script>

<div class="card"><h3 class="help" title="{{t('tip_amount')}}">{{t('give_gold')}}</h3>
<form method="post" action="{{url_for('action')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input type="hidden" name="pid" value="{{p.id}}"><input type="hidden" name="cmd" value="GOLD">
<select name="preset" title="{{t('tip_amount')}}">
{% for label, amt in gold_presets %}<option value="{{amt}}">{{label}}</option>{% endfor %}
<option value="custom">✏️ …</option>
</select>
<input name="custom_amt" placeholder="{{t('amount')}}" title="{{t('tip_amount')}}">
<button class="big" title="{{t('tip_amount')}}">{{t('send_gold')}}</button></form></div>

<div class="card"><h3 class="help" title="{{t('tip_level')}}">{{t('set_level')}}</h3>
<form method="post" action="{{url_for('action')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input type="hidden" name="pid" value="{{p.id}}"><input type="hidden" name="cmd" value="LEVEL">
<input name="arg1" type="number" min="1" max="{{max_level}}" placeholder="{{t('new_level').format(max=max_level)}}" title="{{t('tip_level')}}" required>
<button class="big" title="{{t('tip_level')}}">{{t('change_level')}}</button></form></div>

<div class="card"><h3 class="help" title="{{t('tip_teleport')}}">{{t('teleport')}} <span class="muted">{{t('ingame_only')}}</span></h3>
<form method="post" action="{{url_for('action')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input type="hidden" name="pid" value="{{p.id}}"><input type="hidden" name="cmd" value="WARP">
<select name="preset" title="{{t('tip_teleport')}}">
{% for label, xy in warp_presets %}<option value="{{xy}}">{{label}}</option>{% endfor %}
</select>
<button class="big" title="{{t('tip_teleport')}}">{{t('teleport')}}</button></form></div>

<div class="card"><h3 class="help" title="{{t('tip_speed')}}">{{t('speed')}} <span class="muted">{{t('ingame_only')}}</span></h3>
<form method="post" action="{{url_for('action')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input type="hidden" name="pid" value="{{p.id}}"><input type="hidden" name="cmd" value="SPEED">
<select name="arg1" title="{{t('tip_speed')}}">
{% for label, spd in speed_presets %}<option value="{{spd}}">{{label}}</option>{% endfor %}
</select>
<button class="big" title="{{t('tip_speed')}}">{{t('apply')}}</button></form></div>

<div class="card"><h3 class="help" title="{{t('tip_gm')}}">{{t('gm_title')}}</h3>
{% if gm_rank %}<p class="muted">{{t('gm_now').format(rank=gm_rank_label(gm_rank))}}</p>{% endif %}
<form method="post" action="{{url_for('set_gm')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<input type="hidden" name="pid" value="{{p.id}}">
<select name="rank" title="{{t('tip_gm')}}">
<option value=""{% if not gm_rank %} selected{% endif %}>{{t('gm_none')}}</option>
{% for rank, label in gm_ranks %}<option value="{{rank}}"{% if rank == gm_rank %} selected{% endif %}>{{label}}</option>{% endfor %}
</select>
<button class="big" title="{{t('tip_gm')}}">{{t('gm_apply')}}</button></form></div>

<div class="card"><h3 class="help" title="{{t('tip_inv')}}">🎒 {{t('inv_title')}}{% if inv %} <span class="muted">({{inv|length}})</span>{% endif %}</h3>
{% if inv is none %}<p class="muted">{{t('db_down')}}</p>
{% elif not inv %}<p class="muted">{{t('inv_empty')}}</p>
{% else %}
<div style="max-height:320px;overflow:auto">
<table>
{% for it in inv %}<tr><td>{{it.name}}</td><td class="muted">×{{it.count}}</td><td class="muted">{{it.window}}</td></tr>{% endfor %}
</table></div>
{% endif %}</div>""")

TPL_RATES = BASE.replace("__BODY__", """
<p><a href="{{url_for('dash')}}">{{t('back_players')}}</a></p>
<div class="card">
<h3>{{t('rates_nav')}}</h3>
<p class="muted">{{t('rates_intro')}}</p>
<p><span class="badge">⭐ {{t('rates_exp')}} {{cur['exp']}}%</span>
   <span class="badge">🎁 {{t('rates_drop')}} {{cur['drop']}}%</span>
   <span class="badge">💰 {{t('rates_yang')}} {{cur['yang']}}%</span></p>
<p class="muted">{{t('rates_current')}}</p>
{% if state_msg %}<p class="muted"><b>{{t('rates_st')}}:</b> {{state_msg}}</p>{% endif %}
</div>

<div class="card"><h3>{{t('rates_presets')}}</h3>
<p class="muted">{{t('rates_presets_hint')}}</p>
{% for key, e, d, y in presets %}
<button type="button" class="big" onclick="m2rates({{e}},{{d}},{{y}})">{{t(key)}}</button>
{% endfor %}
</div>

<div class="card">
<form method="post">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<h3>⭐ {{t('rates_exp')}}</h3>
<p class="muted">{{t('rates_exp_help')}}</p>
<input id="r_exp" name="exp" type="number" min="1" max="10000" step="1"
       value="{{cur['exp']}}" placeholder="{{t('rates_percent')}}" required>
<h3 style="margin-top:18px">🎁 {{t('rates_drop')}}</h3>
<p class="muted">{{t('rates_drop_help')}}</p>
<input id="r_drop" name="drop" type="number" min="1" max="10000" step="1"
       value="{{cur['drop']}}" placeholder="{{t('rates_percent')}}" required>
<h3 style="margin-top:18px">💰 {{t('rates_yang')}}</h3>
<p class="muted">{{t('rates_yang_help')}}</p>
<input id="r_yang" name="yang" type="number" min="1" max="10000" step="1"
       value="{{cur['yang']}}" placeholder="{{t('rates_percent')}}" required>
<button class="big" style="margin-top:18px">{{t('rates_save')}}</button>
</form></div>
<script>
function m2rates(e,d,y){
  document.getElementById('r_exp').value=e;
  document.getElementById('r_drop').value=d;
  document.getElementById('r_yang').value=y;
}
</script>""")

# every state apply_rates.sh can leave behind has a sentence of its own
RATE_STATES = ("running", "ok", "unsupported", "failed", "no_restart")

MAP_I18N = {
 "pl": {
  "title":"Mapa świata na żywo — Chunjo","live":"NA ŻYWO (1,5 s)","subtitle":"Interaktywny podgląd pozycji i rozwoju botów w czasie rzeczywistym",
  "player_panel":"Panel graczy","play_browser":"Graj w przeglądarce","show_bots":"Pokaż boty","names_levels":"Nicki i poziomy","pt_only":"Tylko w grupie (PT)",
  "level":"Poziom","all":"Wszystkie","map":"Mapa","m1":"M1 — Joan","m2":"M2 — Bokjung","m3":"M3 — Waryong","monkey":"Łatwy Loch Małp","orc":"Dolina Orków","desert":"Pustynia Yongbi","heat":"Mapa cieplna","heat_deaths":"Zgony botów","heat_metins":"Rozbite metiny","search":"🔍 Szukaj bota (np. botarek)...",
  "solo_bot":"Bot solo","party_bot":"W grupie (PT)","metin_fight":"Walka z Metinem","loading":"Ładowanie...","world_stats":"Statystyki świata","active_bots":"Aktywne boty",
  "in_parties":"W grupach (PT)","avg_level":"Średni poziom","max_level":"Maks. poziom","rankings":"Rankingi botów","rank_level":"Poziom","rank_weapon":"Broń","rank_armor":"Zbroja",
  "rank_weapon30":"Bronie 30 Lv","rank_items":"Przedmioty","rank_horse":"Koń","rank_biologist":"Biolog","rank_hunting":"Polowanie","rank_empty":"Brak danych rankingu.","rank_show":"Pokaż","rank_search":"Szukaj w rankingu...","none":"Brak","items_short":"przedm.",
  "horse_lv":"Koń Lv","visible":"Widocznych","characters":"postaci","in_group":"W grupie [PT]","solo":"Solo","player":"GRACZ","bot":"Bot","class":"Klasa","action":"Akcja","status":"Status","personality":"Osobowość","ambition":"Ambicja","current_goal":"Aktualny cel",
  "coordinates":"Koordynaty","open_inventory":"Kliknij, aby otworzyć ekwipunek i EQ","loading_character":"Ładowanie ekwipunku i statystyk postaci","error":"Błąd","not_found":"Nie znaleziono danych",
  "teleport_me":"Teleportuj moją postać w grze (1 klik)","position":"Pozycja","horse":"Koń","biologist":"Biolog","bio_stage":"Etap Biologa","hunting":"Polowanie","no_data":"Brak danych",
  "stats":"Statystyki","unspent_stats":"Nierozdane: {n} pkt statystyk","skills":"Umiejętności","profession_none":"Nie wybrano","profession_pending":"Profesja nie została jeszcze wybrana.",
  "unspent_skills":"Nierozdane: {n} pkt umiejętności","equipped":"Założony ekwipunek (EQ)","weapon":"Broń","armor":"Zbroja","helmet":"Hełm","shield":"Tarcza","bracelet":"Bransoleta",
  "boots":"Buty","necklace":"Naszyjnik","earrings":"Kolczyki","empty":"Puste","inventory":"Zawartość ekwipunku","items_count":"przedmiotów","inventory_empty":"Ekwipunek jest pusty.","quantity":"Ilość",
  "event_log":"Dziennik zdarzeń bota (logi na żywo)","track_live":"Śledź na żywo","copy_logs":"Kopiuj logi","loading_logs":"Ładowanie logów postaci","no_logs":"Brak najświeższych wpisów w logach dla tej postaci.",
  "log_error":"Błąd odczytu logów","network_error":"Błąd sieci","teleporting":"Teleportowanie Twojej postaci w grze...","teleported":"Przeteleportowano {name} do bota w grze!","you":"Cię","failure":"Niepowodzenie",
  "copied":"Skopiowano","paste":"wklej w grze [Enter] → Ctrl+V → [Enter]","solo_exp":"Solo — zdobywanie doświadczenia","party_exp":"[PT] Zdobywanie doświadczenia w grupie","metin_hunt":"Polowanie na Metiny",
  "character_missing":"Postać nie znaleziona","bio_not_started":"Pierwsza misja jeszcze nierozpoczęta","bio_completed":"Ukończono: {name}","bio_next":"Następna misja od Lv {level}: {name}",
  "bio_all":"Wszystkie podstawowe misje ukończone","bio_complete":"komplet","bio_in_progress":"w toku"
 },
 "en": {
  "title":"Live world map — Chunjo","live":"LIVE (1.5 s)","subtitle":"Interactive real-time view of bot positions and progression",
  "player_panel":"Player panel","play_browser":"Play in browser","show_bots":"Show bots","names_levels":"Names and levels","pt_only":"Party only (PT)",
  "level":"Level","all":"All","map":"Map","m1":"M1 — Joan","m2":"M2 — Bokjung","m3":"M3 — Waryong","monkey":"Easy Monkey Dungeon","orc":"Orc Valley","desert":"Yongbi Desert","heat":"Heatmap","heat_deaths":"Bot deaths","heat_metins":"Metins broken","search":"🔍 Find a bot (e.g. botarek)...",
  "solo_bot":"Solo bot","party_bot":"In party (PT)","metin_fight":"Fighting a Metin","loading":"Loading...","world_stats":"World statistics","active_bots":"Active bots",
  "in_parties":"In parties (PT)","avg_level":"Average level","max_level":"Max level","rankings":"Bot rankings","rank_level":"Level","rank_weapon":"Weapon","rank_armor":"Armour",
  "rank_weapon30":"Lv 30 Weapons","rank_items":"Items","rank_horse":"Horse","rank_biologist":"Biologist","rank_hunting":"Hunting","rank_empty":"No ranking data.","rank_show":"Show","rank_search":"Search ranking...","none":"None","items_short":"items",
  "horse_lv":"Horse Lv","visible":"Visible","characters":"characters","in_group":"In party [PT]","solo":"Solo","player":"PLAYER","bot":"Bot","class":"Class","action":"Action","status":"Status","personality":"Personality","ambition":"Ambition","current_goal":"Current goal",
  "coordinates":"Coordinates","open_inventory":"Click to open inventory and equipment","loading_character":"Loading character equipment and statistics","error":"Error","not_found":"No data found",
  "teleport_me":"Teleport my in-game character (one click)","position":"Position","horse":"Horse","biologist":"Biologist","bio_stage":"Biologist stage","hunting":"Hunting","no_data":"No data",
  "stats":"Statistics","unspent_stats":"Unspent: {n} stat points","skills":"Skills","profession_none":"Not selected","profession_pending":"The profession has not been selected yet.",
  "unspent_skills":"Unspent: {n} skill points","equipped":"Equipped items","weapon":"Weapon","armor":"Armour","helmet":"Helmet","shield":"Shield","bracelet":"Bracelet",
  "boots":"Boots","necklace":"Necklace","earrings":"Earrings","empty":"Empty","inventory":"Inventory contents","items_count":"items","inventory_empty":"The inventory is empty.","quantity":"Quantity",
  "event_log":"Bot event log (live)","track_live":"Track live","copy_logs":"Copy logs","loading_logs":"Loading logs for","no_logs":"No recent log entries for this character.",
  "log_error":"Log read error","network_error":"Network error","teleporting":"Teleporting your in-game character...","teleported":"Teleported {name} to the bot in game!","you":"you","failure":"Failure",
  "copied":"Copied","paste":"paste in game [Enter] → Ctrl+V → [Enter]","solo_exp":"Solo levelling","party_exp":"[PT] Party levelling","metin_hunt":"Hunting Metins",
  "character_missing":"Character not found","bio_not_started":"The first mission has not started yet","bio_completed":"Completed: {name}","bio_next":"Next mission at Lv {level}: {name}",
  "bio_all":"All basic missions completed","bio_complete":"complete","bio_in_progress":"in progress"
 }
}

def map_i18n(language=None):
    language = language or (lang() if has_request_context() else "en")
    return MAP_I18N.get(language, MAP_I18N["en"])

JOB_NAMES_MAP = {
 "pl": {0:"Wojownik (M)",4:"Wojowniczka (K)",1:"Ninja (M)",5:"Ninja (K)",
        2:"Sura (M)",6:"Sura (K)",3:"Szaman (M)",7:"Szamanka (K)"},
 "en": {0:"Warrior (M)",4:"Warrior (F)",1:"Ninja (M)",5:"Ninja (F)",
        2:"Sura (M)",6:"Sura (F)",3:"Shaman (M)",7:"Shaman (F)"},
}
BIOLOGIST_NAMES_EN = {
 "make_herb_lv4":"Peach Blossom","make_herb_lv7":"Bellflower",
 "make_herb_lv10":"Kaki Blossom","make_herb_lv15":"Gango Root",
 "make_herb_lv20":"Lilac","make_herb_lv25":"Tue Mushroom",
}

def localized_job_name(job, language=None):
    language = language or (lang() if has_request_context() else "en")
    names = JOB_NAMES_MAP.get(language, JOB_NAMES_MAP["en"])
    return names.get(int(job or 0), "Wojownik" if language == "pl" else "Warrior")

def localized_biologist_name(quest_name, polish_name, language=None):
    language = language or (lang() if has_request_context() else "en")
    return polish_name if language == "pl" else BIOLOGIST_NAMES_EN.get(quest_name, polish_name)


TPL_LIVE_MAP = BASE.replace("__BODY__", """
<div style="max-width:1240px;margin:0 auto">
  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:14px;flex-wrap:wrap;gap:10px">
    <div>
      <h2 style="margin:0;color:var(--gold2);display:flex;align-items:center;gap:10px;font-size:22px">
        🗺️ {{m.title}}
        <span class="badge" style="font-size:12px;background:rgba(46,204,113,0.15);color:#2ecc71;border:1px solid #2ecc71;padding:3px 8px">
          <span class="dot on"></span> {{m.live}}
        </span>
      </h2>
      <p class="muted" style="margin:4px 0 0;font-size:13px">{{m.subtitle}}</p>
    </div>
    <div style="display:flex;gap:8px">
      <a href="{{url_for('dash')}}" class="btn">← {{m.player_panel}}</a>
      {% if browser_ready %}<a href="{{play_url}}" target="_blank" class="btn" style="background:#27ae60">🎮 {{m.play_browser}}</a>{% endif %}
    </div>
  </div>

  <!-- Controls Toolbar -->
  <div class="card" style="margin-bottom:14px;padding:12px 16px">
    <div style="display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:12px">
      <!-- Checkboxes -->
      <div style="display:flex;flex-wrap:wrap;align-items:center;gap:18px">
        <label style="display:flex;align-items:center;gap:6px;cursor:pointer;font-weight:600;font-size:13px">
          <input type="checkbox" id="toggleBots" checked onchange="renderMap()"> 🤖 {{m.show_bots}}
        </label>
        <label style="display:flex;align-items:center;gap:6px;cursor:pointer;font-weight:600;font-size:13px">
          <input type="checkbox" id="toggleLabels" checked onchange="renderMap()"> 🏷️ {{m.names_levels}}
        </label>
        <label style="display:flex;align-items:center;gap:6px;cursor:pointer;font-weight:600;font-size:13px">
          <input type="checkbox" id="togglePT" onchange="renderMap()"> 👥 {{m.pt_only}}
        </label>
      </div>

      <!-- Level Filter -->
      <div style="display:flex;align-items:center;gap:6px">
        <span class="muted" style="font-size:13px">{{m.level}}:</span>
        <div style="display:flex;gap:4px" id="lvlFilterGroup">
          <button type="button" class="btn lvl-btn active" style="padding:4px 8px;font-size:12px" onclick="setLevelFilter('all', this)">{{m.all}}</button>
          <button type="button" class="btn lvl-btn" style="padding:4px 8px;font-size:12px" onclick="setLevelFilter('1-5', this)">1-5</button>
          <button type="button" class="btn lvl-btn" style="padding:4px 8px;font-size:12px" onclick="setLevelFilter('6-10', this)">6-10</button>
          <button type="button" class="btn lvl-btn" style="padding:4px 8px;font-size:12px" onclick="setLevelFilter('11-15', this)">11-15</button>
          <button type="button" class="btn lvl-btn" style="padding:4px 8px;font-size:12px" onclick="setLevelFilter('16+', this)">16+</button>
        </div>
      </div>

      <div style="display:flex;align-items:center;gap:6px">
        <span class="muted" style="font-size:13px">{{m.map}}:</span>
        <select id="mapFilter" onchange="setMapFilter(this.value)" style="width:auto;padding:6px 9px;font-size:12px;margin:0">
          <option value="21">{{m.m1}}</option><option value="23">{{m.m2}}</option>
          <option value="24">{{m.m3}}</option><option value="25">{{m.monkey}}</option>
          <option value="64">{{m.orc}}</option><option value="63">{{m.desert}}</option>
        </select>
      </div>

      <div style="display:flex;align-items:center;gap:6px">
        <label style="display:flex;align-items:center;gap:5px;cursor:pointer">
          <input type="checkbox" id="toggleHeat" onchange="loadHeatmap()">
          <span class="muted" style="font-size:13px">🔥 {{m.heat}}</span>
        </label>
        <select id="heatKind" onchange="loadHeatmap()" style="width:auto;padding:6px 9px;font-size:12px;margin:0">
          <option value="deaths">{{m.heat_deaths}}</option>
          <option value="metins">{{m.heat_metins}}</option>
        </select>
        <span id="heatSummary" class="muted" style="font-size:12px"></span>
      </div>

      <!-- Search Box -->
      <div>
        <input type="text" id="searchInput" placeholder="{{m.search}}"
               style="padding:6px 12px;font-size:13px;border-radius:6px;background:#15120a;border:1px solid #3d3522;color:#fff;width:200px"
               oninput="renderMap()">
      </div>
    </div>
  </div>

  <!-- Map and Sidebar Layout -->
  <div style="display:grid;grid-template-columns:1fr 340px;gap:16px;align-items:start">
    
    <!-- Map Container -->
    <div class="card" style="padding:8px;position:relative;background:#0a0805;border:2px solid #3d3522;border-radius:10px">
      <div id="mapViewport" style="position:relative;width:100%;height:680px;background:#0e0c08 radial-gradient(circle at center, #18130a 0%, #070604 100%);border-radius:6px;overflow:hidden;box-shadow:inset 0 0 30px rgba(0,0,0,0.95);border:1px solid #2e2617">
        <!-- Markers layer -->
        <!-- Heat layer sits under the markers so a bot is never hidden by it -->
        <div id="heatLayer" style="position:absolute;inset:0;z-index:1"></div>
        <div id="markersLayer" style="position:absolute;inset:0;z-index:2"></div>
        <!-- Tooltip -->
        <div id="mapTooltip" style="display:none;position:absolute;z-index:200;background:rgba(18,15,10,0.96);border:1px solid var(--gold);padding:10px 14px;border-radius:8px;box-shadow:0 8px 24px rgba(0,0,0,0.85);pointer-events:none;min-width:200px;font-size:12px;color:#fff;backdrop-filter:blur(6px)">
        </div>
      </div>
      <div style="display:flex;justify-content:space-between;align-items:center;margin-top:8px;padding:0 6px;font-size:12px;color:var(--muted)">
        <div style="display:flex;gap:12px;align-items:center">
          <span><b style="color:#2ecc71">●</b> {{m.solo_bot}}</span>
          <span><b style="color:#a855f7">●</b> {{m.party_bot}}</span>
          <span><b style="color:#eab308">●</b> {{m.metin_fight}}</span>
        </div>
        <div id="visibleCountBadge" style="font-weight:700;color:var(--gold2)">{{m.loading}}</div>
      </div>
    </div>

    <!-- Sidebar Stats & Leaderboard -->
    <div style="display:flex;flex-direction:column;gap:14px">
      <div class="card" style="padding:14px">
        <h4 style="margin:0 0 10px;color:var(--gold2)">📊 {{m.world_stats}}</h4>
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px;text-align:center">
          <div style="background:#17130c;padding:8px;border-radius:6px;border:1px solid #2e2617">
            <div style="font-size:22px;font-weight:700;color:var(--gold)" id="statTotalBots">0</div>
            <div style="font-size:11px;color:var(--muted)">{{m.active_bots}}</div>
          </div>
          <div style="background:#17130c;padding:8px;border-radius:6px;border:1px solid #2e2617">
            <div style="font-size:22px;font-weight:700;color:#a855f7" id="statInParty">0</div>
            <div style="font-size:11px;color:var(--muted)">{{m.in_parties}}</div>
          </div>
          <div style="background:#17130c;padding:8px;border-radius:6px;border:1px solid #2e2617">
            <div style="font-size:22px;font-weight:700;color:#2ecc71" id="statAvgLevel">0</div>
            <div style="font-size:11px;color:var(--muted)">{{m.avg_level}}</div>
          </div>
          <div style="background:#17130c;padding:8px;border-radius:6px;border:1px solid #2e2617">
            <div style="font-size:22px;font-weight:700;color:#e67e22" id="statMaxLevel">0</div>
            <div style="font-size:11px;color:var(--muted)">{{m.max_level}}</div>
          </div>
        </div>
      </div>

      <div class="card" style="padding:14px">
        <h4 style="margin:0 0 10px;color:var(--gold2)">🏆 {{m.rankings}}</h4>
        <div style="display:flex;gap:4px;margin-bottom:10px;flex-wrap:wrap">
          <button type="button" class="btn btn-sm rank-tab active" onclick="setRankCategory('level', this)" style="font-size:11px;padding:3px 6px">⭐ {{m.rank_level}}</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('gold', this)" style="font-size:11px;padding:3px 6px">💰 Yang</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('weapon30', this)" style="font-size:11px;padding:3px 6px">⚔️ {{m.rank_weapon30}}</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('weapon', this)" style="font-size:11px;padding:3px 6px">🗡️ {{m.rank_weapon}}</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('armor', this)" style="font-size:11px;padding:3px 6px">🛡️ {{m.rank_armor}}</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('items', this)" style="font-size:11px;padding:3px 6px">🎒 {{m.rank_items}}</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('horse', this)" style="font-size:11px;padding:3px 6px">🐴 {{m.rank_horse}}</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('biologist', this)" style="font-size:11px;padding:3px 6px">🌿 {{m.rank_biologist}}</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('hunting', this)" style="font-size:11px;padding:3px 6px">🎯 {{m.rank_hunting}}</button>
        </div>
        <div style="display:grid;grid-template-columns:minmax(0,1fr) auto;gap:6px;margin-bottom:10px;align-items:center">
          <input id="rankSearch" type="search" placeholder="🔍 {{m.rank_search}}" oninput="renderRankings()" style="min-width:0;margin:0;padding:7px 8px;font-size:11px">
          <label style="display:flex;align-items:center;gap:5px;font-size:11px;color:var(--muted);white-space:nowrap">
            {{m.rank_show}}
            <select id="rankLimit" onchange="setRankLimit(this.value)" style="margin:0;padding:6px 24px 6px 7px;font-size:11px;width:auto">
              <option value="15">15</option>
              <option value="30">30</option>
              <option value="50">50</option>
              <option value="100">100</option>
            </select>
          </label>
        </div>
        <div style="max-height:410px;overflow-y:auto" id="topBotsList">
          <p class="muted" style="font-size:12px;text-align:center">{{m.loading}}</p>
        </div>
      </div>
    </div>

  </div>
</div>

<style>
.lvl-btn.active, .rank-tab.active { background: var(--gold) !important; color: #000 !important; font-weight: 700; }
.heat-dot {
  position: absolute;
  transform: translate(-50%, -50%);
  border-radius: 50%;
  pointer-events: none;
}
.bot-marker {
  position: absolute;
  transform: translate(-50%, -50%);
  cursor: pointer;
  transition: left 0.8s ease-out, top 0.8s ease-out;
  display: flex;
  flex-direction: column;
  align-items: center;
  z-index: 10;
}
.bot-marker:hover { z-index: 150; }
.bot-dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  box-shadow: 0 0 8px rgba(0,0,0,0.8);
  border: 1.5px solid #fff;
  transition: transform 0.2s;
}
.bot-dot.player {
  width: 14px;
  height: 14px;
  background: #ef4444;
  border-color: #ffd700;
  box-shadow: 0 0 12px #ef4444;
  animation: pulse 1.5s infinite;
}
.bot-dot.pt { background: #a855f7; box-shadow: 0 0 8px #a855f7; }
.bot-dot.solo { background: #2ecc71; box-shadow: 0 0 6px #2ecc71; }
.bot-dot.metin { background: #eab308; box-shadow: 0 0 10px #eab308; }
.bot-label {
  font-size: 9px;
  font-weight: 700;
  color: #fff;
  background: rgba(0,0,0,0.75);
  padding: 1px 4px;
  border-radius: 3px;
  white-space: nowrap;
  margin-top: 2px;
  border: 1px solid rgba(255,255,255,0.2);
  pointer-events: none;
}
@keyframes pulse {
  0% { transform: scale(1); }
  50% { transform: scale(1.35); }
  100% { transform: scale(1); }
}
.rank-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 6px 8px;
  border-radius: 6px;
  margin-bottom: 4px;
  background: #14110a;
  border: 1px solid #231d10;
  font-size: 12px;
  cursor: pointer;
  transition: background 0.15s, border-color 0.15s;
}
.rank-row:hover { background: #241e12; border-color: var(--gold); }

/* Modal Styles */
.modal-overlay {
  display: none;
  position: fixed;
  top: 0; left: 0; right: 0; bottom: 0;
  background: rgba(0,0,0,0.85);
  backdrop-filter: blur(4px);
  z-index: 9999;
  justify-content: center;
  align-items: center;
}
.modal-box {
  background: #14110a;
  border: 2px solid var(--gold);
  box-shadow: 0 0 30px rgba(0,0,0,0.9), 0 0 15px rgba(212,175,55,0.3);
  border-radius: 10px;
  width: 90%;
  max-width: 980px;
  max-height: 90vh;
  overflow-y: auto;
  padding: 20px;
  color: #e5e5e5;
  position: relative;
}
.m2-modal-columns {
  display: grid;
  grid-template-columns: minmax(360px, 1fr) 220px;
  gap: 16px;
  align-items: start;
}
@media (max-width: 820px) {
  .m2-modal-columns {
    grid-template-columns: 1fr;
  }
}
.m2-inv-window {
  background: #110e0a linear-gradient(180deg, #1e1810 0%, #0d0a07 100%);
  border: 2px solid #5a4b32;
  box-shadow: inset 0 0 10px #000, 0 4px 20px rgba(0,0,0,0.8);
  border-radius: 6px;
  padding: 10px;
  width: 220px;
  box-sizing: border-box;
  user-select: none;
  font-family: Arial, sans-serif;
  margin: 0 auto;
}
.m2-window-title {
  text-align: center;
  color: #e6ca65;
  font-size: 12px;
  font-weight: 700;
  letter-spacing: 0.5px;
  padding-bottom: 6px;
  margin-bottom: 8px;
  border-bottom: 1px solid #3d3119;
  text-shadow: 1px 1px 2px #000;
}
.m2-equip-container {
  position: relative;
  width: 200px;
  height: 156px;
  background: #0d0b08;
  border: 1px solid #2e2413;
  border-radius: 4px;
  margin-bottom: 10px;
  box-shadow: inset 0 0 15px rgba(0,0,0,0.9);
}
.m2-equip-silhouette {
  position: absolute;
  top: 0; left: 0; right: 0; bottom: 0;
  background: radial-gradient(circle at 50% 50%, rgba(212,175,55,0.06) 0%, rgba(0,0,0,0.6) 80%);
  pointer-events: none;
}
.m2-equip-slot {
  position: absolute;
  background: rgba(14, 11, 7, 0.9);
  border: 1px solid #4a3c22;
  box-shadow: inset 0 0 6px #000;
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  box-sizing: border-box;
  transition: border-color 0.15s, box-shadow 0.15s;
}
.m2-equip-slot:hover {
  border-color: #ffd700;
  box-shadow: 0 0 8px rgba(255, 215, 0, 0.5), inset 0 0 6px #000;
}
.m2-equip-watermark {
  font-size: 13px;
  opacity: 0.3;
  filter: grayscale(1);
}
.m2-inv-tabs {
  display: flex;
  gap: 4px;
  margin-bottom: 6px;
}
.m2-tab-btn {
  flex: 1;
  background: #231b11;
  border: 1px solid #4a3c22;
  color: #a89070;
  font-weight: 700;
  font-size: 11px;
  padding: 4px 0;
  border-radius: 3px;
  cursor: pointer;
  text-align: center;
  transition: all 0.15s;
}
.m2-tab-btn.active {
  background: #c5a059 linear-gradient(180deg, #d4af37 0%, #aa8230 100%);
  color: #000;
  border-color: #ffe082;
  box-shadow: 0 0 6px rgba(212,175,55,0.4);
}
.m2-tab-btn:hover:not(.active) {
  background: #332717;
  color: #ffd700;
}
.m2-grid-frame {
  position: relative;
  width: 170px;
  height: 306px;
  margin: 0 auto;
  background: #0a0806;
  border: 1px solid #3d3119;
  box-shadow: inset 0 0 10px #000;
}
.m2-grid-bg {
  display: grid;
  grid-template-columns: repeat(5, 34px);
  grid-template-rows: repeat(9, 34px);
  position: absolute;
  top: 0; left: 0;
  width: 170px; height: 306px;
}
.m2-grid-cell {
  width: 34px;
  height: 34px;
  border-right: 1px solid #221a10;
  border-bottom: 1px solid #221a10;
  box-sizing: border-box;
}
.m2-item-overlay {
  position: absolute;
  top: 0; left: 0;
  width: 170px; height: 306px;
  pointer-events: none;
}
.m2-item-entity {
  position: absolute;
  pointer-events: auto;
  cursor: pointer;
  box-sizing: border-box;
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid transparent;
  transition: border-color 0.15s;
}
.m2-item-entity:hover {
  border-color: rgba(255, 215, 0, 0.8);
  background: rgba(255, 215, 0, 0.08);
  box-shadow: 0 0 8px rgba(255, 215, 0, 0.4);
}
.m2-item-count {
  position: absolute;
  right: 2px;
  bottom: 1px;
  color: #fff;
  font-size: 10px;
  font-weight: 700;
  text-shadow: 1px 1px 0 #000, -1px -1px 0 #000, 1px -1px 0 #000, -1px 1px 0 #000;
  pointer-events: none;
}
.m2-yang-box {
  margin-top: 8px;
  background: #0a0806;
  border: 1px solid #3d3119;
  border-radius: 4px;
  padding: 4px 8px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-size: 11px;
  box-shadow: inset 0 0 6px #000;
}
.m2-yang-val {
  color: #facc15;
  font-weight: 700;
  font-family: monospace;
  font-size: 12px;
}
/* Metin2 In-Game Style Floating Tooltip */
#m2ItemTooltip {
  position: fixed;
  display: none;
  z-index: 100000;
  pointer-events: none;
  background: rgba(12, 10, 8, 0.95);
  border: 1.5px solid #c5a059;
  box-shadow: 0 4px 20px rgba(0,0,0,0.9), inset 0 0 10px rgba(0,0,0,0.8);
  border-radius: 4px;
  padding: 8px 12px;
  min-width: 170px;
  max-width: 260px;
  color: #d1d5db;
  font-family: Arial, sans-serif;
  font-size: 11px;
  line-height: 1.35;
}
.m2-tt-name {
  color: #ffd700;
  font-size: 13px;
  font-weight: 700;
  text-align: center;
  margin-bottom: 4px;
  text-shadow: 1px 1px 2px #000;
}
.m2-tt-divider {
  height: 1px;
  background: #4a3c22;
  margin: 4px 0;
}
.m2-tt-stat {
  color: #93c5fd;
}
.m2-tt-bonus {
  color: #4ade80;
}
.m2-tt-socket {
  display: flex;
  align-items: center;
  gap: 4px;
  margin-top: 2px;
  font-size: 10px;
}
.bot-build-grid {
  display: grid;
  grid-template-columns: minmax(200px, 0.8fr) minmax(240px, 1.2fr);
  gap: 10px;
  margin: 12px 0 14px;
}
.bot-build-panel {
  background: #1a150e;
  border: 1px solid #332814;
  border-radius: 6px;
  padding: 9px;
}
.stat-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 6px;
  margin-top: 7px;
}
.stat-box {
  background: #11100b;
  border: 1px solid #3d3119;
  border-radius: 5px;
  padding: 7px 4px;
  text-align: center;
}
.skill-row {
  display: grid;
  grid-template-columns: 1fr auto;
  gap: 8px;
  padding: 4px 6px;
  margin-top: 4px;
  background: #11100b;
  border-radius: 4px;
  font-size: 11px;
}
@media (max-width: 720px) {
  .bot-build-grid { grid-template-columns: 1fr; }
}
.modal-close-btn {
  position: absolute;
  top: 12px;
  right: 14px;
  background: none;
  border: none;
  color: #aaa;
  font-size: 20px;
  cursor: pointer;
}
.modal-close-btn:hover { color: #fff; }
</style>

<!-- Floating Metin2 In-Game Item Tooltip -->
<div id="m2ItemTooltip"></div>

<!-- Bot Detail & Inventory Modal -->
<div id="botModal" class="modal-overlay" onclick="if(event.target===this)closeBotModal()">
  <div class="modal-box">
    <button class="modal-close-btn" onclick="closeBotModal()">&times;</button>
    <div id="botModalContent">
      <p class="muted" style="text-align:center">{{m.loading_character}}...</p>
    </div>
  </div>
</div>

<script>
var I18N = {{m|tojson}};
var g_bots = [];
var g_selectedLevel = 'all';
var g_selectedMap = 21;
var g_selectedRankCategory = 'level';
var g_rankLimit = 15;
var g_rankData = [];
var g_highlightedId = null;

function escapeHtml(value) {
  return String(value === undefined || value === null ? '' : value)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#039;');
}

function setLevelFilter(lvl, btn) {
  g_selectedLevel = lvl;
  document.querySelectorAll('.lvl-btn').forEach(function(b){ b.classList.remove('active'); });
  btn.classList.add('active');
  renderMap();
}

function setMapFilter(mapIndex) {
  g_selectedMap = parseInt(mapIndex, 10) || 21;
  renderMap();
  loadHeatmap();
}

var g_heatCells = [];
var g_heatMax = 0;

function heatEnabled() {
  var el = document.getElementById('toggleHeat');
  return !!(el && el.checked);
}

function loadHeatmap() {
  var layer = document.getElementById('heatLayer');
  if (!layer) return;
  var summary = document.getElementById('heatSummary');
  if (!heatEnabled()) {
    g_heatCells = []; g_heatMax = 0; layer.innerHTML = '';
    if (summary) summary.textContent = '';
    return;
  }
  var kindEl = document.getElementById('heatKind');
  var kind = kindEl ? kindEl.value : 'deaths';
  fetch('/api/bot_heatmap?map=' + g_selectedMap + '&kind=' + encodeURIComponent(kind))
    .then(function(r){ return r.json(); })
    .then(function(d){
      g_heatCells = (d && d.cells) ? d.cells : [];
      g_heatMax = (d && d.max) ? d.max : 0;
      renderHeat();
      if (summary) summary.textContent = (d && d.total) ? ('∑ ' + d.total) : '—';
    })
    .catch(function(){ layer.innerHTML = ''; if (summary) summary.textContent = ''; });
}

function renderHeat() {
  var layer = document.getElementById('heatLayer');
  if (!layer) return;
  if (!heatEnabled() || !g_heatCells.length || !g_heatMax) { layer.innerHTML = ''; return; }
  var kindEl = document.getElementById('heatKind');
  var rgb = (kindEl && kindEl.value === 'metins') ? '255,105,205' : '255,72,58';
  var html = '';
  for (var i = 0; i < g_heatCells.length; i++) {
    var c = g_heatCells[i];
    // Square root, so the visible area tracks the count instead of the radius --
    // otherwise one busy cell swallows the whole map.
    var w = Math.sqrt(c.n / g_heatMax);
    var size = (16 + w * 52).toFixed(0);
    var alpha = (0.16 + w * 0.58).toFixed(2);
    html += '<div class="heat-dot" style="left:' + c.px + '%;top:' + c.py + '%;width:' + size +
            'px;height:' + size + 'px;background:radial-gradient(circle,rgba(' + rgb + ',' + alpha +
            ') 0%,rgba(' + rgb + ',0) 70%)"></div>';
  }
  layer.innerHTML = html;
}

function setRankCategory(cat, btn) {
  g_selectedRankCategory = cat;
  document.querySelectorAll('.rank-tab').forEach(function(b){ b.classList.remove('active'); });
  btn.classList.add('active');
  fetchRankings();
}

function setRankLimit(value) {
  var parsed = parseInt(value, 10);
  g_rankLimit = [15, 30, 50, 100].indexOf(parsed) >= 0 ? parsed : 15;
  fetchRankings();
}

function fetchBotPositions() {
  fetch('/api/bot_positions')
    .then(function(res) { return res.json(); })
    .then(function(data) {
      if (data && data.ok) {
        g_bots = data.bots;
        renderMap();
        updateStats();
      }
    })
    .catch(function(err) { console.error('Map fetch error:', err); });
}

function fetchRankings() {
  fetch('/api/bot_rankings?type=' + encodeURIComponent(g_selectedRankCategory) +
        '&limit=' + g_rankLimit)
    .then(function(res) { return res.json(); })
    .then(function(data) {
      if (data && data.ok) {
        g_rankData = data.rankings || [];
        renderRankings();
      }
    })
    .catch(function(err) { console.error('Ranking fetch error:', err); });
}

function updateStats() {
  var total = g_bots.length;
  var inPt = 0;
  var sumLvl = 0;
  var maxLvl = 1;

  g_bots.forEach(function(b) {
    if (b.in_pt) inPt++;
    sumLvl += b.level;
    if (b.level > maxLvl) maxLvl = b.level;
  });

  document.getElementById('statTotalBots').innerText = total;
  document.getElementById('statInParty').innerText = inPt;
  document.getElementById('statAvgLevel').innerText = total > 0 ? (sumLvl / total).toFixed(1) : 0;
  document.getElementById('statMaxLevel').innerText = maxLvl;

  if (g_rankData.length === 0) {
    fetchRankings();
  }
}

function renderRankings() {
  var listEl = document.getElementById('topBotsList');
  if (!listEl) return;
  if (!g_rankData || g_rankData.length === 0) {
    listEl.innerHTML = '<p class="muted" style="font-size:12px;text-align:center">' + I18N.rank_empty + '</p>';
    return;
  }

  var rankHtml = '';
  var rankSearchEl = document.getElementById('rankSearch');
  var rankSearch = rankSearchEl ? rankSearchEl.value.toLowerCase().trim() : '';
  g_rankData.forEach(function(b, idx) {
    if (rankSearch && ((b.name || '') + ' ' + (b.job || '')).toLowerCase().indexOf(rankSearch) < 0) {
      return;
    }
    var ptBadge = b.in_pt ? '<span style="color:#a855f7;font-weight:700">[PT]</span> ' : '';
    var detailStr = '';
    if (g_selectedRankCategory === 'gold') {
      detailStr = '<span style="color:#eab308;font-weight:700">' + (b.gold || 0).toLocaleString() + ' Yang</span>';
    } else if (g_selectedRankCategory === 'weapon30') {
      var srStr = b.sr !== undefined ? '<span style="color:#4ade80;font-weight:700">ŚR: ' + (b.sr > 0 ? '+' : '') + b.sr + '%</span>' : '';
      var umStr = b.um !== undefined && b.um !== 0 ? ' <span style="color:#38bdf8;font-weight:700">UM: ' + (b.um > 0 ? '+' : '') + b.um + '%</span>' : '';
      var winBadge = b.item_window === 'EQUIPMENT' ? '<span style="background:#15803d;color:#fff;font-size:9px;padding:1px 4px;border-radius:3px;margin-left:4px">EQ</span>'
                                                   : '<span style="background:#374151;color:#bbb;font-size:9px;padding:1px 4px;border-radius:3px;margin-left:4px">Plecak</span>';
      var iconUrl = b.weapon_vnum ? getItemIconUrl(b.weapon_vnum) : null;
      var iconImg = iconUrl ? '<img src="' + iconUrl + '" style="width:16px;height:16px;vertical-align:middle;margin-right:4px;image-rendering:pixelated">' : '';
      detailStr = '<div>' + iconImg + '<span style="color:#ffd700;font-weight:700">' + (b.weapon_name || 'Broń 30 Lv') + '</span> ' + winBadge + '</div><div>' + srStr + umStr + '</div>';
    } else if (g_selectedRankCategory === 'weapon') {
      detailStr = '<span style="color:#38bdf8;font-weight:700">' + (b.weapon_name || I18N.none) + '</span>';
    } else if (g_selectedRankCategory === 'armor') {
      detailStr = '<span style="color:#2ecc71;font-weight:700">' + (b.armor_name || I18N.none) + '</span>';
    } else if (g_selectedRankCategory === 'items') {
      detailStr = '<span style="color:#f59e0b;font-weight:700">' + (b.item_count || 0) + ' ' + I18N.items_short + '</span>';
    } else if (g_selectedRankCategory === 'horse') {
      detailStr = '<span style="color:#c084fc;font-weight:700">' + I18N.horse_lv + ' ' + (b.horse_level || 0) + '</span>';
    } else if (g_selectedRankCategory === 'biologist') {
      detailStr = '<span style="color:#4ade80;font-weight:700">' + (b.biologist_label || '0/6') + '</span>';
    } else if (g_selectedRankCategory === 'hunting') {
      detailStr = '<span style="color:#fb923c;font-weight:700">' + (b.hunting_label || I18N.none) + '</span>';
    } else {
      detailStr = '<span style="color:var(--gold);font-weight:700">Lv ' + b.level + '</span>';
    }

    rankHtml += '<div class="rank-row" onclick="openBotModal(' + b.id + ')">' +
                '<div><b>#' + (idx+1) + '</b> ' + ptBadge + '<b>' + b.name + '</b> <span class="muted">(' + b.job + ')</span></div>' +
                '<div style="text-align:right">' + detailStr + ' <span style="font-size:10px;color:#888">🔍</span></div>' +
                '</div>';
  });
  listEl.innerHTML = rankHtml || '<p class="muted" style="font-size:12px;text-align:center">' + I18N.rank_empty + '</p>';
}

function highlightBot(pid) {
  g_highlightedId = pid;
  renderMap();
  var marker = document.getElementById('marker_' + pid);
  if (marker) {
    marker.scrollIntoView({ behavior: 'smooth', block: 'center' });
    showTooltip(pid);
  }
}

function renderMap() {
  var layer = document.getElementById('markersLayer');
  if (!layer) return;

  var showBots = document.getElementById('toggleBots').checked;
  var showLabels = document.getElementById('toggleLabels').checked;
  var showPTOnly = document.getElementById('togglePT').checked;
  var searchStr = document.getElementById('searchInput').value.toLowerCase().trim();

  var html = '';
  var visibleCount = 0;

  g_bots.forEach(function(b) {
    if (parseInt(b.map_index, 10) !== g_selectedMap) return;
    var isBot = b.is_bot;
    if (isBot && !showBots) return;
    if (showPTOnly && !b.in_pt) return;

    if (g_selectedLevel === '1-5' && (b.level < 1 || b.level > 5)) return;
    if (g_selectedLevel === '6-10' && (b.level < 6 || b.level > 10)) return;
    if (g_selectedLevel === '11-15' && (b.level < 11 || b.level > 15)) return;
    if (g_selectedLevel === '16+' && b.level < 16) return;

    if (searchStr && b.name.toLowerCase().indexOf(searchStr) === -1) return;

    visibleCount++;

    var dotClass = b.in_pt ? 'pt' : 'solo';
    var isHighlighted = (b.id === g_highlightedId);
    var transformStyle = isHighlighted ? 'transform:translate(-50%, -50%) scale(1.6);z-index:180;' : '';
    var pxVal = b.px !== undefined ? b.px : b.percent_x;
    var pyVal = b.py !== undefined ? b.py : b.percent_y;

    html += '<div id="marker_' + b.id + '" class="bot-marker" style="left:' + pxVal + '%;top:' + pyVal + '%;' + transformStyle + '"' +
            ' onmouseenter="showTooltip(' + b.id + ', event)" onmouseleave="hideTooltip()"' +
            ' onclick="openBotModal(' + b.id + ')">' +
            '<div class="bot-dot ' + dotClass + '"></div>';

    if (showLabels || isHighlighted) {
      html += '<div class="bot-label">' + escapeHtml(b.name) + ' (' + b.level + ')</div>';
    }

    html += '</div>';
  });

  layer.innerHTML = html;
  document.getElementById('visibleCountBadge').innerText = I18N.visible + ': ' + visibleCount + ' ' + I18N.characters;
}

function showTooltip(pid, ev) {
  var bot = g_bots.find(function(x) { return x.id === pid; });
  if (!bot) return;

  var tt = document.getElementById('mapTooltip');
  if (!tt) return;

  var ptStr = bot.in_pt ? '<span style="color:#a855f7;font-weight:700">' + I18N.in_group + '</span>' : '<span style="color:#2ecc71">' + I18N.solo + '</span>';
  var statusBadge = bot.is_player ? '<span style="color:#ef4444;font-weight:700">👑 ' + I18N.player + '</span>' : '🤖 ' + I18N.bot;
  var actionStr = escapeHtml(bot.action || (bot.in_pt ? I18N.party_exp : I18N.solo_exp));

  tt.innerHTML = '<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px">' +
                 '<b style="font-size:14px;color:var(--gold2)">' + escapeHtml(bot.name) + '</b> ' + statusBadge + '</div>' +
                 '<div style="color:#ddd;margin-bottom:4px"><b>' + I18N.class + ':</b> ' + escapeHtml(bot.job) + ' &nbsp;|&nbsp; <b>' + I18N.level + ':</b> ' + bot.level + '</div>' +
                 '<div style="color:#c084fc;margin-bottom:4px"><b>' + I18N.personality + ':</b> ' + escapeHtml(bot.personality) + '</div>' +
                 '<div style="color:#86efac;margin-bottom:4px"><b>' + I18N.ambition + ':</b> ' + escapeHtml(bot.ambition) + ' &nbsp;|&nbsp; <b>' + I18N.current_goal + ':</b> ' + escapeHtml(bot.goal) + '</div>' +
                 '<div style="color:#ffd700;margin-bottom:4px"><b>' + I18N.action + ':</b> ' + actionStr + '</div>' +
                 '<div style="color:#ddd;margin-bottom:4px"><b>' + I18N.status + ':</b> ' + ptStr + '</div>' +
                 '<div style="color:#aaa;margin-bottom:4px"><b>' + I18N.coordinates + ':</b> (' + bot.x + ', ' + bot.y + ')</div>' +
                 '<div style="color:#eab308;margin-bottom:4px"><b>Yang:</b> ' + (bot.gold || 0).toLocaleString() + '</div>' +
                 '<div style="color:#38bdf8;font-size:10px;font-weight:700;margin-top:4px">👉 ' + I18N.open_inventory + '</div>';

  var pxVal = bot.px !== undefined ? bot.px : (bot.percent_x || 50);
  var pyVal = bot.py !== undefined ? bot.py : (bot.percent_y || 50);
  tt.style.display = 'block';
  tt.style.left = Math.min(pxVal + 2, 75) + '%';
  tt.style.top = Math.min(pyVal + 2, 80) + '%';
}

function hideTooltip() {
  var tt = document.getElementById('mapTooltip');
  if (tt) tt.style.display = 'none';
}

var g_itemDefs = null;
var g_itemIcons = null;
var g_currentInvData = null;
var g_currentInvTab = 0;

// Preload item definitions and icons lookup table
fetch('/static/item_defs.json')
  .then(function(res) { return res.json(); })
  .then(function(data) { g_itemDefs = data; })
  .catch(function(err) { console.warn('Could not load item_defs.json:', err); });

fetch('/static/item_icons.json')
  .then(function(res) { return res.json(); })
  .then(function(data) { g_itemIcons = data; })
  .catch(function(err) { console.warn('Could not load item_icons.json:', err); });

var ATTR_NAMES = {
  1: {pl: "Max PŻ", en: "Max HP"},
  2: {pl: "Max PE", en: "Max MP"},
  3: {pl: "Energia Życiowa", en: "Vitality"},
  4: {pl: "Inteligencja", en: "Intelligence"},
  5: {pl: "Siła", en: "Strength"},
  6: {pl: "Zręczność", en: "Dexterity"},
  7: {pl: "Szybkość Ataku", en: "Attack Speed"},
  8: {pl: "Szybkość Poruszania się", en: "Movement Speed"},
  9: {pl: "Szybkość Zaklęcia", en: "Casting Speed"},
  10: {pl: "Regeneracja PŻ", en: "HP Regen"},
  11: {pl: "Regeneracja PE", en: "MP Regen"},
  12: {pl: "Szansa na Otrucie", en: "Poisoning Chance"},
  13: {pl: "Szansa na Omdlenie", en: "Stun Chance"},
  14: {pl: "Szansa na Spowolnienie", en: "Slow Chance"},
  15: {pl: "Szansa na Cios Krytyczny", en: "Critical Hit"},
  16: {pl: "Szansa na Przeszywający Cios", en: "Piercing Hit"},
  17: {pl: "Silny przeciwko Półludziom", en: "Strong vs Half-Humans"},
  18: {pl: "Silny przeciwko Zwierzętom", en: "Strong vs Animals"},
  19: {pl: "Silny przeciwko Orkom", en: "Strong vs Orcs"},
  20: {pl: "Silny przeciwko Mistykom", en: "Strong vs Esoterics"},
  21: {pl: "Silny przeciwko Nieumarłym", en: "Strong vs Undead"},
  22: {pl: "Silny przeciwko Diabłom", en: "Strong vs Devils"},
  23: {pl: "Obrażenia absorbowane przez PŻ", en: "Damage absorbed by HP"},
  24: {pl: "Obrażenia absorbowane przez PE", en: "Damage absorbed by MP"},
  25: {pl: "Szansa na kradzież PE", en: "Chance to steal MP"},
  27: {pl: "Szansa na Blok Ciosów", en: "Block Chance"},
  28: {pl: "Szansa na Unik Strzał", en: "Dodge Arrows"},
  29: {pl: "Odporność na Miecze", en: "Sword Defense"},
  30: {pl: "Odporność na Broń Dwuręczną", en: "Two-Handed Defense"},
  31: {pl: "Odporność na Sztylety", en: "Dagger Defense"},
  32: {pl: "Odporność na Dzwony", en: "Bell Defense"},
  33: {pl: "Odporność na Wachlarze", en: "Fan Defense"},
  34: {pl: "Odporność na Strzały", en: "Arrow Resistance"},
  35: {pl: "Odporność na Ogień", en: "Fire Resistance"},
  36: {pl: "Odporność na Błyskawice", en: "Lightning Resistance"},
  37: {pl: "Odporność na Magię", en: "Magic Resistance"},
  38: {pl: "Odporność na Wiatr", en: "Wind Resistance"},
  41: {pl: "Odporność na Trucizny", en: "Poison Resistance"},
  43: {pl: "Bonus Doświadczenia", en: "EXP Bonus"},
  44: {pl: "Szansa na podwójną ilość Yang", en: "Double Yang Drop"},
  45: {pl: "Szansa na podwójną ilość Przedmiotów", en: "Double Item Drop"},
  53: {pl: "Wartość Ataku", en: "Attack Value"},
  54: {pl: "Obrona", en: "Defense"},
  71: {pl: "Średnie Obrażenia", en: "Average Damage"},
  72: {pl: "Obrażenia Umiejętności", en: "Skill Damage"}
};

function getItemIconUrl(vnum) {
  var vStr = String(vnum);
  var iconFile = (g_itemIcons && g_itemIcons[vStr]) ? g_itemIcons[vStr] : null;
  if (!iconFile) {
    // Default fallback calculation: 5 digits zero-padded
    var padded = ('00000' + vnum).slice(-5);
    iconFile = padded + '.png';
  }
  return '/static/icons/' + iconFile;
}

function showEquipTooltip(ev, el) {
  var key = el.getAttribute('data-eqslot');
  if (g_currentInvData && g_currentInvData.equipment && g_currentInvData.equipment[key]) {
    showItemTooltip(ev, g_currentInvData.equipment[key]);
  }
}

function showItemTooltip(ev, item) {
  var tt = document.getElementById('m2ItemTooltip');
  if (!tt || !item) return;

  var lg = I18N.language || 'pl';
  var def = (g_itemDefs && g_itemDefs[String(item.vnum)]) || {};
  var name = item.name || def.name || ('Item #' + item.vnum);

  var html = '<div class="m2-tt-name">' + name + '</div>';

  if (def.level && def.level > 0) {
    html += '<div style="color:#a1a1aa;font-size:10px">' + (lg === 'pl' ? 'Wymagany Poziom: ' : 'Required Level: ') + '<b style="color:#e5e7eb">' + def.level + '</b></div>';
  }

  // Combat Stats (Attack / Defense)
  var hasStats = false;
  var statHtml = '';
  if (def.type === 1) { // Weapon
    var minAtt = (def.value3 || 0) + (def.value5 || 0);
    var maxAtt = (def.value4 || 0) + (def.value5 || 0);
    if (minAtt > 0 || maxAtt > 0) {
      statHtml += '<div class="m2-tt-stat">' + (lg === 'pl' ? 'Wartość Ataku: ' : 'Attack Value: ') + minAtt + ' - ' + maxAtt + '</div>';
      hasStats = true;
    }
    var minMag = (def.value1 || 0) + (def.value5 || 0);
    var maxMag = (def.value2 || 0) + (def.value5 || 0);
    if (minMag > 0 || maxMag > 0) {
      statHtml += '<div class="m2-tt-stat">' + (lg === 'pl' ? 'Wartość Magicznego Ataku: ' : 'Magic Attack Value: ') + minMag + ' - ' + maxMag + '</div>';
      hasStats = true;
    }
    if (def.value0 > 0) {
      statHtml += '<div class="m2-tt-stat">' + (lg === 'pl' ? 'Szybkość Ataku: +' : 'Attack Speed: +') + def.value0 + '%</div>';
      hasStats = true;
    }
  } else if (def.type === 2) { // Armor / Equip
    var defVal = 0;
    if (def.subtype === 0) defVal = (def.value1 || 0) + (def.value5 || 0) * 2; // Body
    else if (def.subtype === 1) defVal = (def.value1 || 0) + (def.value5 || 0); // Head
    else if (def.subtype === 2) defVal = (def.value1 || 0) + (def.value5 || 0) * 2; // Shield
    else if (def.subtype === 4) defVal = (def.value1 || 0) + (def.value5 || 0); // Boots

    if (defVal > 0) {
      statHtml += '<div class="m2-tt-stat">' + (lg === 'pl' ? 'Obrona: ' : 'Defense: ') + defVal + '</div>';
      hasStats = true;
    }
    if (def.value0 > 0) {
      statHtml += '<div class="m2-tt-stat">' + (lg === 'pl' ? 'Szybkość Ruchu: ' : 'Movement Speed: ') + (def.subtype === 0 ? '-' : '+') + def.value0 + '%</div>';
      hasStats = true;
    }
  }

  // Base Apply bonuses
  if (def.apply && def.apply.length > 0) {
    def.apply.forEach(function(ap) {
      if (ap.type && ap.val) {
        var apName = ATTR_NAMES[ap.type] ? (ATTR_NAMES[ap.type][lg] || ATTR_NAMES[ap.type].en) : ('Bonus #' + ap.type);
        statHtml += '<div class="m2-tt-stat">' + apName + ': +' + ap.val + (ap.type === 1 || ap.type === 2 || ap.type === 53 || ap.type === 54 ? '' : '%') + '</div>';
        hasStats = true;
      }
    });
  }

  if (hasStats) {
    html += '<div class="m2-tt-divider"></div>' + statHtml;
  }

  // Bonuses (1-7)
  if (item.attrs && item.attrs.length > 0) {
    html += '<div class="m2-tt-divider"></div>';
    item.attrs.forEach(function(a) {
      var aName = ATTR_NAMES[a.type] ? (ATTR_NAMES[a.type][lg] || ATTR_NAMES[a.type].en) : ('Bonus #' + a.type);
      var sign = a.val > 0 ? '+' : '';
      var pct = (a.type === 1 || a.type === 2 || a.type === 53 || a.type === 54) ? '' : '%';
      html += '<div class="m2-tt-bonus">' + aName + ' ' + sign + a.val + pct + '</div>';
    });
  }

  // Sockets (Spirit Stones / KD)
  if (item.sockets && item.sockets.length > 0) {
    var hasSock = false;
    var sockHtml = '<div class="m2-tt-divider"></div>';
    item.sockets.forEach(function(sVal, sIdx) {
      if (sVal === 0 && def.type !== 1 && def.type !== 2) return;
      hasSock = true;
      if (sVal === 0) {
        sockHtml += '<div class="m2-tt-socket" style="color:#71717a">⚪ ' + (lg === 'pl' ? 'Pęknięty Kamień' : 'Broken Stone') + '</div>';
      } else if (sVal === 1) {
        sockHtml += '<div class="m2-tt-socket" style="color:#94a3b8">⚪ ' + (lg === 'pl' ? 'Czysty Slot' : 'Empty Socket') + '</div>';
      } else if (sVal >= 28000 && sVal <= 28999) {
        var sDef = (g_itemDefs && g_itemDefs[String(sVal)]) || {};
        var sName = sDef.name || ('Kamień Duszy #' + sVal);
        sockHtml += '<div class="m2-tt-socket" style="color:#38bdf8">💎 ' + sName + '</div>';
      } else if (sVal > 1) {
        sockHtml += '<div class="m2-tt-socket" style="color:#94a3b8">⚙️ #' + sVal + '</div>';
      }
    });
    if (hasSock) html += sockHtml;
  }

  tt.innerHTML = html;
  tt.style.display = 'block';
  moveItemTooltip(ev);
}

function moveItemTooltip(ev) {
  var tt = document.getElementById('m2ItemTooltip');
  if (!tt || tt.style.display === 'none') return;
  var x = ev.clientX + 14;
  var y = ev.clientY + 14;
  var maxX = window.innerWidth - tt.offsetWidth - 20;
  var maxY = window.innerHeight - tt.offsetHeight - 20;
  tt.style.left = Math.min(x, maxX) + 'px';
  tt.style.top = Math.min(y, maxY) + 'px';
}

function hideItemTooltip() {
  var tt = document.getElementById('m2ItemTooltip');
  if (tt) tt.style.display = 'none';
}

function switchInvTab(tabIdx) {
  g_currentInvTab = tabIdx;
  document.querySelectorAll('.m2-tab-btn').forEach(function(b, idx) {
    if (idx === tabIdx) b.classList.add('active');
    else b.classList.remove('active');
  });
  if (g_currentInvData) {
    renderInventoryGrid(g_currentInvData.inventory || []);
  }
}

function renderInventoryGrid(invItems) {
  var overlay = document.getElementById('m2InvItemOverlay');
  if (!overlay) return;
  overlay.innerHTML = '';

  var basePos = g_currentInvTab * 45;
  var endPos = basePos + 45;

  invItems.forEach(function(it) {
    if (it.pos < basePos || it.pos >= endPos) return;

    var relPos = it.pos - basePos;
    var col = relPos % 5;
    var row = Math.floor(relPos / 5);

    var def = (g_itemDefs && g_itemDefs[String(it.vnum)]) || {};
    var size = def.size || 1;
    if (size < 1) size = 1;
    if (size > 3) size = 3;

    var leftPx = col * 34;
    var topPx = row * 34;
    var widthPx = 34;
    var heightPx = size * 34;

    var el = document.createElement('div');
    el.className = 'm2-item-entity';
    el.style.left = leftPx + 'px';
    el.style.top = topPx + 'px';
    el.style.width = widthPx + 'px';
    el.style.height = heightPx + 'px';

    var iconUrl = getItemIconUrl(it.vnum);
    var img = document.createElement('img');
    img.src = iconUrl;
    img.style.maxWidth = '32px';
    img.style.maxHeight = (size * 32) + 'px';
    img.style.imageRendering = 'pixelated';
    img.draggable = false;
    el.appendChild(img);

    if (it.count > 1) {
      var badge = document.createElement('span');
      badge.className = 'm2-item-count';
      badge.innerText = it.count;
      el.appendChild(badge);
    }

    el.onmouseenter = function(ev) { showItemTooltip(ev, it); };
    el.onmousemove = function(ev) { moveItemTooltip(ev); };
    el.onmouseleave = function() { hideItemTooltip(); };

    overlay.appendChild(el);
  });
}

function openBotModal(pid) {
  var modal = document.getElementById('botModal');
  var content = document.getElementById('botModalContent');
  modal.style.display = 'flex';
  content.innerHTML = '<p class="muted" style="text-align:center;padding:20px">' + I18N.loading_character + ' #' + pid + '...</p>';

  fetch('/api/bot_inventory/' + pid)
    .then(function(res) { return res.json(); })
    .then(function(data) {
      if (!data || !data.ok) {
        content.innerHTML = '<p style="color:#ef4444;text-align:center">' + I18N.error + ': ' + (data.error || I18N.not_found) + '</p>';
        return;
      }

      g_currentInvData = data;
      var p = data.player;
      var eq = data.equipment || {};
      var inv = data.inventory || [];
      var stats = p.stats || {};
      var skills = p.skills || [];
      var isBot = p.name.startsWith('bot');
      var typeBadge = isBot ? '<span style="background:#2ecc71;color:#000;padding:2px 6px;border-radius:4px;font-size:11px;font-weight:700">BOT</span>'
                            : '<span style="background:#ef4444;color:#fff;padding:2px 6px;border-radius:4px;font-size:11px;font-weight:700">GRACZ</span>';

      var html = '<div class="m2-modal-columns">';

      // LEFT COLUMN: Character Details & Stats & Logs
      html += '<div>';
      html += '<div style="display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #332814;padding-bottom:10px;margin-bottom:12px">' +
              '<div><h3 style="margin:0;color:var(--gold2);font-size:18px">' + escapeHtml(p.name) + ' <span style="font-size:13px;color:#aaa">Lv ' + p.level + ' ' + escapeHtml(p.job_name) + '</span></h3></div>' +
              '<div>' + typeBadge + '</div>' +
              '</div>';

      // GM Teleport bar
      var wx = Math.floor(p.x / 100);
      var wy = Math.floor(p.y / 100);
      html += '<div style="margin-bottom:12px">' +
              '<button type="button" class="btn btn-sm" onclick="warpMeToBot(' + p.x + ',' + p.y + ')" style="width:100%;margin-bottom:8px;background:#16a34a;color:#fff;font-weight:700;padding:8px 12px;border:none;border-radius:6px;cursor:pointer;font-size:13px;box-shadow:0 0 10px rgba(22,163,74,0.5)">' +
              '⚡ ' + I18N.teleport_me +
              '</button>' +
              '<div style="display:flex;gap:8px">' +
              '<button type="button" class="btn btn-sm" onclick="copyWarp(' + wx + ',' + wy + ')" style="flex:1;background:#2563eb;color:#fff;font-weight:700;padding:6px;border:none;border-radius:4px;cursor:pointer;font-size:12px">' +
              '📋 /warp ' + wx + ' ' + wy +
              '</button>' +
              '<button type="button" class="btn btn-sm" onclick="copyWarp(' + p.x + ',' + p.y + ')" style="background:#374151;color:#fff;font-weight:700;padding:6px 12px;border:none;border-radius:4px;cursor:pointer;font-size:12px">' +
              '🌐 /warp ' + p.x + ' ' + p.y +
              '</button>' +
              '</div>' +
              '</div>';

      html += '<div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:14px;font-size:12px;background:#1a150e;padding:10px;border-radius:6px">' +
              '<div><b>HP:</b> <span style="color:#ef4444">' + (p.hp || 0) + '</span> / <b>MP:</b> <span style="color:#38bdf8">' + (p.mp || 0) + '</span></div>' +
              '<div><b>Yang:</b> <span style="color:#eab308;font-weight:700">' + (p.gold || 0).toLocaleString() + '</span></div>' +
              '<div><b>' + I18N.position + ':</b> (' + p.x + ', ' + p.y + ')</div>' +
              '<div><b>' + I18N.personality + ':</b> <span style="color:#c084fc;font-weight:700">' + escapeHtml(p.personality) + '</span></div>' +
              '<div><b>' + I18N.ambition + ':</b> <span style="color:#86efac;font-weight:700">' + escapeHtml(p.ambition) + '</span></div>' +
              '<div style="grid-column:1 / -1"><b>' + I18N.current_goal + ':</b> <span style="color:#60a5fa;font-weight:700">' + escapeHtml(p.goal) + '</span></div>' +
              '<div style="grid-column:1 / -1"><b>' + I18N.action + ':</b> <span style="color:#ffd700">' + escapeHtml(p.action) + '</span></div>' +
              '<div><b>' + I18N.horse + ':</b> <span style="color:#c084fc;font-weight:700">Lv ' + (p.horse_level || 0) + '</span></div>' +
              '<div><b>' + I18N.biologist + ':</b> <span style="color:#4ade80;font-weight:700">' + (p.biologist_completed || 0) + '/6</span></div>' +
              '<div style="grid-column:1 / -1"><b>' + I18N.bio_stage + ':</b> <span style="color:#86efac">' + (p.biologist_label || I18N.no_data) + '</span></div>' +
              '<div style="grid-column:1 / -1"><b>' + I18N.hunting + ':</b> <span style="color:#fb923c">' + (p.hunting_label || I18N.no_data) + '</span></div>' +
              '</div>';

      // Character build: stats & skills
      html += '<div class="bot-build-grid">' +
              '<div class="bot-build-panel">' +
              '<h4 style="margin:0;color:var(--gold)">📊 ' + I18N.stats + '</h4>' +
              '<div class="stat-grid">';
      ['STR', 'VIT', 'DEX', 'INT'].forEach(function(key) {
        html += '<div class="stat-box"><div style="font-size:10px;color:var(--muted)">' + key + '</div>' +
                '<div style="font-size:17px;font-weight:700;color:#f8fafc">' + (stats[key] || 0) + '</div></div>';
      });
      html += '</div><div class="muted" style="font-size:10px;margin-top:7px">' +
              I18N.unspent_stats.replace('{n}', p.stat_point || 0) + '</div></div>' +
              '<div class="bot-build-panel">' +
              '<div style="display:flex;justify-content:space-between;gap:8px;align-items:center">' +
              '<h4 style="margin:0;color:var(--gold)">✨ ' + I18N.skills + '</h4>' +
              '<span style="font-size:11px;color:#38bdf8;font-weight:700">' +
              (p.profession_name || I18N.profession_none) + '</span></div>';
      if (skills.length === 0) {
        html += '<div class="muted" style="font-size:11px;margin-top:8px">' + I18N.profession_pending + '</div>';
      } else {
        skills.forEach(function(skill) {
          var rankColor = skill.rank === '0' ? '#71717a' :
                          (skill.rank.charAt(0) === 'P' ? '#f472b6' :
                          (skill.rank.charAt(0) === 'G' ? '#c084fc' :
                          (skill.rank.charAt(0) === 'M' ? '#38bdf8' : '#f8fafc')));
          html += '<div class="skill-row"><span>' + skill.name + '</span>' +
                  '<b style="color:' + rankColor + '">' + skill.rank + '</b></div>';
        });
      }
      html += '<div class="muted" style="font-size:10px;margin-top:7px">' +
              I18N.unspent_skills.replace('{n}', p.skill_point || 0) + '</div></div></div>';

      // Live Bot Logs section
      html += '<div style="margin-top:14px;border-top:1px solid #332814;padding-top:10px">' +
              '<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px">' +
              '<h4 style="margin:0;color:var(--gold);font-size:13px">📜 ' + I18N.event_log + '</h4>' +
              '<div style="display:flex;gap:10px;align-items:center">' +
              '<label style="font-size:11px;display:flex;align-items:center;gap:4px;cursor:pointer;color:#38bdf8">' +
              '<input type="checkbox" id="chkAutoLog" checked onchange="toggleLogTracking()"> 🔄 ' + I18N.track_live +
              '</label>' +
              '<button type="button" onclick="copyBotLogs()" style="padding:2px 8px;font-size:11px;background:#334155;color:#fff;border:none;border-radius:4px;cursor:pointer">📋 ' + I18N.copy_logs + '</button>' +
              '</div>' +
              '</div>' +
              '<div id="botLogsConsole" style="background:#09090b;border:1px solid #27272a;border-radius:6px;padding:8px;max-height:140px;overflow-y:auto;font-family:monospace;font-size:10px;color:#a1a1aa;line-height:1.4">' +
              I18N.loading_logs + ' ' + p.name + '...' +
              '</div>' +
              '</div>';

      html += '</div>'; // End Left Column

      // RIGHT COLUMN: Authentic Metin2 Inventory Window
      html += '<div>';
      html += '<div class="m2-inv-window">' +
              '<div class="m2-window-title">⚔️ ' + (I18N.inventory || 'Ekwipunek') + '</div>';

      // Equipment Section with Character Silhouette
      html += '<div class="m2-equip-container">' +
              '<div class="m2-equip-silhouette"></div>';

      var equipCoords = {
        weapon: { left: 10, top: 10, w: 34, h: 102, watermark: '🗡️' },
        body:   { left: 54, top: 44, w: 34, h: 68,  watermark: '🛡️' },
        head:   { left: 54, top: 6,  w: 34, h: 34,  watermark: '🪖' },
        foots:  { left: 54, top: 116,w: 34, h: 34,  watermark: '🥾' },
        ear:    { left: 98, top: 6,  w: 34, h: 34,  watermark: '👂' },
        shield: { left: 98, top: 44, w: 34, h: 34,  watermark: '🛡️' },
        wrist:  { left: 98, top: 80, w: 34, h: 34,  watermark: '💍' },
        neck:   { left: 98, top: 116,w: 34, h: 34,  watermark: '📿' }
      };

      Object.keys(equipCoords).forEach(function(slotKey) {
        var cfg = equipCoords[slotKey];
        var it = eq[slotKey];
        var inner = '';
        var hoverAttr = '';
        if (it) {
          var iconUrl = getItemIconUrl(it.vnum);
          var def = (g_itemDefs && g_itemDefs[String(it.vnum)]) || {};
          var size = def.size || 1;
          inner = '<img src="' + iconUrl + '" style="max-width:32px;max-height:' + (size*32) + 'px;image-rendering:pixelated" draggable="false">';
          hoverAttr = ' data-eqslot="' + slotKey + '" onmouseenter="showEquipTooltip(event, this)" onmousemove="moveItemTooltip(event)" onmouseleave="hideItemTooltip()"';
        } else {
          inner = '<span class="m2-equip-watermark">' + cfg.watermark + '</span>';
        }
        html += '<div class="m2-equip-slot" style="left:' + cfg.left + 'px;top:' + cfg.top + 'px;width:' + cfg.w + 'px;height:' + cfg.h + 'px"' + hoverAttr + '>' + inner + '</div>';
      });

      html += '</div>'; // End Equipment Section

      // Inventory Tabs (Tab I & Tab II)
      html += '<div class="m2-inv-tabs">' +
              '<button type="button" class="m2-tab-btn active" onclick="switchInvTab(0)">I</button>' +
              '<button type="button" class="m2-tab-btn" onclick="switchInvTab(1)">II</button>' +
              '</div>';

      // 5x9 Inventory Grid Frame
      html += '<div class="m2-grid-frame">' +
              '<div class="m2-grid-bg">';
      for (var c = 0; c < 45; c++) {
        html += '<div class="m2-grid-cell"></div>';
      }
      html += '</div>' +
              '<div id="m2InvItemOverlay" class="m2-item-overlay"></div>' +
              '</div>';

      // Yang display
      html += '<div class="m2-yang-box">' +
              '<span>💰 Yang:</span>' +
              '<span class="m2-yang-val">' + (p.gold || 0).toLocaleString() + '</span>' +
              '</div>';

      html += '</div>'; // End m2-inv-window
      html += '</div>'; // End Right Column

      html += '</div>'; // End m2-modal-columns

      content.innerHTML = html;
      renderInventoryGrid(inv);
      startLogTracking(p.name);
    })
    .catch(function(err) {
      content.innerHTML = '<p style="color:#ef4444;text-align:center">' + I18N.network_error + ': ' + err + '</p>';
    });
}

var g_activeLogBot = null;
var g_logInterval = null;
var g_collectedLogs = [];
var g_seenLogsSet = {};

function fetchBotLogs(botName) {
  var consoleEl = document.getElementById('botLogsConsole');
  if (!consoleEl) return;

  fetch('/api/bot_logs/' + encodeURIComponent(botName))
    .then(function(res) { return res.json(); })
    .then(function(data) {
      if (!consoleEl) return;
      if (data && data.ok && data.logs && data.logs.length > 0) {
        var addedNew = false;
        data.logs.forEach(function(line) {
          if (!g_seenLogsSet[line]) {
            g_seenLogsSet[line] = true;
            g_collectedLogs.push(line);
            addedNew = true;
          }
        });

        if (g_collectedLogs.length > 1000) {
          g_collectedLogs = g_collectedLogs.slice(-1000);
        }

        var logHtml = '';
        g_collectedLogs.forEach(function(line) {
          var color = '#a1a1aa';
          if (line.indexOf('USE_SKILL') !== -1 || line.indexOf('activated self buff') !== -1) color = '#38bdf8';
          else if (line.indexOf('target acquired') !== -1) color = '#eab308';
          else if (line.indexOf('picked up loot') !== -1 || line.indexOf('GIVE_GOLD') !== -1) color = '#2ecc71';
          else if (line.indexOf('used health potion') !== -1 || line.indexOf('used mana potion') !== -1) color = '#f43f5e';
          else if (line.indexOf('SYSERR') !== -1 || line.indexOf('failed') !== -1) color = '#ef4444';
          logHtml += '<div style="color:' + color + ';white-space:nowrap">' + line + '</div>';
        });
        consoleEl.innerHTML = logHtml;
        consoleEl.scrollTop = consoleEl.scrollHeight;
      } else if (g_collectedLogs.length === 0) {
        consoleEl.innerHTML = '<div style="color:#71717a">' + I18N.no_logs + '</div>';
      }
    })
    .catch(function(err) {
      if (consoleEl && g_collectedLogs.length === 0) {
        consoleEl.innerHTML = '<div style="color:#ef4444">' + I18N.log_error + ': ' + err + '</div>';
      }
    });
}

function startLogTracking(botName) {
  if (g_activeLogBot !== botName) {
    g_activeLogBot = botName;
    g_collectedLogs = [];
    g_seenLogsSet = {};
  }
  if (g_logInterval) clearInterval(g_logInterval);
  fetchBotLogs(botName);
  g_logInterval = setInterval(function() {
    var chk = document.getElementById('chkAutoLog');
    if (chk && chk.checked && g_activeLogBot) {
      fetchBotLogs(g_activeLogBot);
    }
  }, 1500);
}

function toggleLogTracking() {
  var chk = document.getElementById('chkAutoLog');
  if (chk && chk.checked && g_activeLogBot) {
    startLogTracking(g_activeLogBot);
  }
}

function copyBotLogs() {
  var consoleEl = document.getElementById('botLogsConsole');
  if (consoleEl) {
    var fullText = g_collectedLogs.length > 0 ? g_collectedLogs.join(String.fromCharCode(10)) : consoleEl.innerText;
    copyCommand(fullText);
  }
}

function closeBotModal() {
  if (g_logInterval) {
    clearInterval(g_logInterval);
    g_logInterval = null;
  }
  g_activeLogBot = null;
  g_collectedLogs = [];
  g_seenLogsSet = {};
  document.getElementById('botModal').style.display = 'none';
}

function copyWarp(x, y) {
  copyCommand('/warp ' + x + ' ' + y);
}

function warpMeToBot(x, y) {
  var t = document.getElementById('mapToast');
  if (t) {
    t.innerText = '⏳ ' + I18N.teleporting;
    t.style.display = 'block';
  }
  fetch('/api/admin/warp_me', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ x: x, y: y, player_name: 'tieru' })
  })
  .then(function(res) { return res.json(); })
  .then(function(data) {
    if (data && data.ok) {
      if (t) {
        t.innerText = '✅ ' + I18N.teleported.replace('{name}', data.name || I18N.you);
        setTimeout(function() { t.style.display = 'none'; }, 4000);
      }
    } else {
      if (t) {
        t.innerText = '❌ ' + I18N.error + ': ' + (data.error || data.status || I18N.failure);
        setTimeout(function() { t.style.display = 'none'; }, 4000);
      }
    }
  })
  .catch(function(err) {
    if (t) {
      t.innerText = '❌ ' + I18N.network_error + ': ' + err;
      setTimeout(function() { t.style.display = 'none'; }, 4000);
    }
  });
}

function copyCommand(cmd) {
  if (navigator.clipboard) {
    navigator.clipboard.writeText(cmd);
  }
  var t = document.getElementById('mapToast');
  if (t) {
    t.innerText = '📋 ' + I18N.copied + ': ' + cmd + ' (' + I18N.paste + ')';
    t.style.display = 'block';
    setTimeout(function() { t.style.display = 'none'; }, 3500);
  }
}

// Initial fetch and 1.5s interval polling
fetchBotPositions();
setInterval(fetchBotPositions, 1500);
</script>
<div id="mapToast" style="display:none;position:fixed;bottom:25px;left:50%;transform:translateX(-50%);background:#1e293b;border:1px solid #38bdf8;color:#f8fafc;padding:10px 20px;border-radius:8px;font-size:13px;font-weight:600;box-shadow:0 10px 25px rgba(0,0,0,0.5);z-index:999999;transition:all 0.3s ease"></div>
""")


@app.route("/live_map")
@app.route("/map")
def live_map():
    language = lang()
    return render_template_string(TPL_LIVE_MAP,
                                  brand=CONF.get("server_name", "Metin2"),
                                  browser_ready=browser_client_ready(),
                                  play_url=play_url(),
                                  m=map_i18n(language),
                                  langs=LANGS,
                                  curlang=language,
                                  is_admin=bool(session.get("auth")))

@app.route("/api/admin/warp_me", methods=["POST"])
def api_admin_warp_me():
    try:
        data = request.get_json(force=True, silent=True) or {}
        target_x = int(data.get("x", 0))
        target_y = int(data.get("y", 0))
        gm_name = data.get("player_name", "tieru")

        # Fallback to the latest active human player
        if not gm_name or gm_name == "auto":
            with db() as c, c.cursor() as cur:
                cur.execute("SELECT name FROM player.player WHERE name NOT LIKE 'bot%' ORDER BY last_play DESC LIMIT 1")
                r = cur.fetchone()
                if r:
                    gm_name = r["name"]
                else:
                    gm_name = "tieru"

        st, qid = queue_and_wait(gm_name, "WARP", target_x, target_y, wait=5.0)
        return jsonify({"ok": True, "status": st, "name": gm_name, "x": target_x, "y": target_y})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500

@app.route("/api/bot_logs/<string:bot_name>")
def api_bot_logs(bot_name):
    try:
        bot_name = bot_name.strip()
        log_files = [
            "/opt/metin2/var/channel1/game1/syslog",
            "/opt/metin2/var/channel1/first/syslog",
            "/opt/metin2/var/channel1/game2/syslog"
        ]
        matched_lines = []
        for log_path in log_files:
            if os.path.exists(log_path):
                try:
                    with open(log_path, "r", encoding="latin-1", errors="ignore") as f:
                        lines = f.readlines()
                        recent = lines[-800:] if len(lines) > 800 else lines
                        for line in recent:
                            if bot_name.lower() in line.lower():
                                matched_lines.append(line.strip())
                except Exception:
                    pass
        last_logs = matched_lines[-60:] if len(matched_lines) > 60 else matched_lines
        return jsonify({"ok": True, "bot_name": bot_name, "count": len(last_logs), "logs": last_logs})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e), "logs": []})

# base_x, base_y, width, height -- read out of each map's Setting.txt
# (BasePosition, and MapSize x 128 x 200 for the extent). Shared by the live
# marker layer and the heatmap so the same world point lands in the same place
# in both.
PLAYERBOT_MAP_BOUNDS = {
    21: (0, 102400, 102400, 128000),        # Joan (Chunjo M1)
    23: (102400, 204800, 102400, 102400),   # Bokjung (Chunjo M2)
    24: (179200, 0, 51200, 51200),          # Waryong (Chunjo M3)
    25: (844800, 435200, 76800, 76800),     # Easy Monkey Dungeon
    64: (256000, 665600, 153600, 153600),   # Orc Valley
    63: (204800, 486400, 153600, 153600),   # Yongbi Desert
}


# The game core already writes every death and every destroyed Metin to
# log.log with world coordinates, so the heatmap needs no extra bookkeeping in
# the server. Rows carry no map index, but the maps occupy disjoint rectangles
# of the world, so the bounding box identifies the map exactly.
PLAYERBOT_HEATMAP_KINDS = {
    "deaths": "DEAD_BY_NPC",
    "metins": "STONE_KILL",
}
PLAYERBOT_HEATMAP_GRID = 44


@app.route("/api/bot_heatmap")
def api_bot_heatmap():
    try:
        map_index = int(request.args.get("map", 21))
    except (TypeError, ValueError):
        map_index = 21
    kind = (request.args.get("kind") or "deaths").strip().lower()
    how = PLAYERBOT_HEATMAP_KINDS.get(kind)
    bounds = PLAYERBOT_MAP_BOUNDS.get(map_index)
    if how is None or bounds is None:
        return jsonify({"ok": False, "cells": [], "max": 0, "total": 0})

    base_x, base_y, width, height = bounds
    cell_w = max(1, width // PLAYERBOT_HEATMAP_GRID)
    cell_h = max(1, height // PLAYERBOT_HEATMAP_GRID)
    try:
        with db() as c, c.cursor() as cur:
            cur.execute(
                """
                SELECT FLOOR((l.x - %s) / %s) AS gx,
                       FLOOR((l.y - %s) / %s) AS gy,
                       COUNT(*) AS n
                FROM log.log l
                WHERE l.type = 'CHARACTER' AND l.how = %s
                  AND l.x >= %s AND l.x < %s
                  AND l.y >= %s AND l.y < %s
                GROUP BY gx, gy
                HAVING n > 0
                """,
                (base_x, cell_w, base_y, cell_h, how,
                 base_x, base_x + width, base_y, base_y + height),
            )
            rows = cur.fetchall()
    except Exception as e:
        return jsonify({"ok": False, "error": str(e), "cells": [], "max": 0, "total": 0})

    cells = []
    peak = 0
    total = 0
    for r in rows:
        n = int(r.get("n") or 0)
        gx = int(r.get("gx") or 0)
        gy = int(r.get("gy") or 0)
        # Centre of the cell, as a percentage of the map, so the overlay lines
        # up with the bot markers whatever the viewport size is.
        px = ((gx + 0.5) * cell_w) / float(width) * 100.0
        py = ((gy + 0.5) * cell_h) / float(height) * 100.0
        if px < 0 or px > 100 or py < 0 or py > 100:
            continue
        cells.append({"px": round(px, 2), "py": round(py, 2), "n": n})
        total += n
        if n > peak:
            peak = n
    return jsonify({"ok": True, "cells": cells, "max": peak, "total": total, "kind": kind})


@app.route("/api/bot_positions")
def api_bot_positions():
    language = lang()
    messages = map_i18n(language)
    live_status = read_playerbot_live_status()
    map_bounds = PLAYERBOT_MAP_BOUNDS
    try:
        with db() as c, c.cursor() as cur:
            # A bot is defined by its canonical account (playerbot_NNN), not by
            # its character name -- renaming a bot in player.player used to drop
            # it off the live map. The name LIKE 'bot%' arm keeps any legacy or
            # hand-made bot visible too.
            cur.execute("""
                SELECT p.id, p.name, p.level, p.job, p.x, p.y, p.hp, p.gold, p.map_index
                FROM player.player p
                LEFT JOIN account.account a ON a.id = p.account_id
                WHERE (LEFT(a.login, 10) = 'playerbot_' OR p.name LIKE 'bot%')
                  AND p.map_index IN (21, 23, 24, 25, 63, 64)
                ORDER BY p.level DESC, p.id ASC
            """)
            rows = cur.fetchall()
            bots = []
            total_count = len(rows)
            for r in rows:
                pid = r.get("id") or 0
                live = live_status.get(int(pid))
                live_labels = playerbot_live_labels(live, language)
                name = r.get("name") or ""
                level = r.get("level") or 1
                job = r.get("job") or 0
                gx = live.get("x") if live else (r.get("x") or 55000)
                gy = live.get("y") if live else (r.get("y") or 160000)
                hp = live.get("hp") if live else (r.get("hp") or 0)
                gold = r.get("gold") or 0
                map_index = int(live.get("map_index") if live else (r.get("map_index") or 21))
                base_x, base_y, width, height = map_bounds.get(map_index, map_bounds[21])

                px = max(0.0, min(100.0, ((float(gx) - base_x) / width) * 100.0))
                py = max(0.0, min(100.0, ((float(gy) - base_y) / height) * 100.0))
                is_bot = True  # every row from the query above is a playerbot
                in_pt = live.get("in_pt") if live else bot_in_party_cohort(pid)

                bots.append({
                    "id": pid,
                    "name": name,
                    "level": level,
                    "job": localized_job_name(job, language),
                    "x": gx,
                    "y": gy,
                    "map_index": map_index,
                    "px": round(px, 1),
                    "py": round(py, 1),
                    "hp": hp,
                    "gold": gold,
                    "in_pt": in_pt,
                    "is_bot": is_bot,
                    "action": live_labels["action"],
                    "personality": live_labels["personality"],
                    "ambition": live_labels["ambition"],
                    "goal": live_labels["goal"],
                    "live": bool(live),
                })
            return jsonify({"ok": True, "count": total_count, "bots": bots})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e), "bots": []})

@app.route("/api/bot_inventory/<int:pid>")
def api_bot_inventory(pid):
    language = lang()
    messages = map_i18n(language)
    live = read_playerbot_live_status().get(int(pid))
    live_labels = playerbot_live_labels(live, language)
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("""
                SELECT id, name, level, job, exp, gold, hp, mp, x, y,
                       horse_level, st, ht, dx, iq, stat_point, skill_point,
                       skill_group, skill_level
                FROM player.player
                WHERE id = %s
            """, (pid,))
            player = cur.fetchone()
            if not player:
                return jsonify({"ok": False, "error": messages["character_missing"]})

            player["job_name"] = localized_job_name(player.get("job", 0), language)
            player["in_pt"] = live.get("in_pt") if live else bot_in_party_cohort(pid)
            if live:
                player["x"] = live.get("x", player.get("x"))
                player["y"] = live.get("y", player.get("y"))
                player["hp"] = live.get("hp", player.get("hp"))
            player["action"] = live_labels["action"]
            player["personality"] = live_labels["personality"]
            player["ambition"] = live_labels["ambition"]
            player["goal"] = live_labels["goal"]
            player["live"] = bool(live)
            raw_skills = player.pop("skill_level", b"")
            profession_names = SKILL_GROUP_NAMES if language == "pl" else SKILL_GROUP_NAMES_EN
            player["profession_name"] = profession_names.get(
                (int(player.get("job") or 0) % 4, int(player.get("skill_group") or 0)),
                messages["profession_none"])
            player["skills"] = parse_player_skills(
                raw_skills, player.get("job"), player.get("skill_group"), language)
            player["stats"] = {
                "STR": int(player.get("st") or 0),
                "VIT": int(player.get("ht") or 0),
                "DEX": int(player.get("dx") or 0),
                "INT": int(player.get("iq") or 0),
            }

            mission_names = tuple(m[0] for m in BIOLOGIST_MISSIONS)
            placeholders = ",".join(["%s"] * len(mission_names))
            quest_sql = """
                SELECT szName, szState, lValue
                FROM player.quest
                WHERE dwPID = %s AND szName IN ({})
            """.format(placeholders)
            cur.execute(quest_sql, (pid,) + mission_names)
            quest_rows = cur.fetchall()
            quest_flags = {
                (row["szName"], row["szState"]): int(row.get("lValue") or 0)
                for row in quest_rows
            }
            completed = 0
            biologist_label = messages["bio_not_started"]
            for quest_name, required_level, item_name, required_count in BIOLOGIST_MISSIONS:
                item_name = localized_biologist_name(quest_name, item_name, language)
                if quest_flags.get((quest_name, "__status")) == BIOLOGIST_COMPLETE_STATE:
                    completed += 1
                    biologist_label = messages["bio_completed"].format(name=item_name)
                    continue
                if int(player.get("level") or 1) >= required_level:
                    accepted = quest_flags.get((quest_name, "collect_count"), 0)
                    biologist_label = "%s: %d/%d" % (item_name, accepted, required_count)
                else:
                    biologist_label = messages["bio_next"].format(
                        level=required_level, name=item_name)
                break
            else:
                biologist_label = messages["bio_all"]
            player["biologist_completed"] = completed
            player["biologist_label"] = biologist_label

            cur.execute("""
                SELECT szState, lValue
                FROM player.quest
                WHERE dwPID = %s AND szName = 'levelup'
                  AND szState IN ('current','select','remain','complete')
            """, (pid,))
            hunting_flags = {
                row["szState"]: int(row.get("lValue") or 0)
                for row in cur.fetchall()
            }
            player["hunting_current"] = hunting_flags.get("current", 0)
            player["hunting_complete"] = hunting_flags.get("complete", 0)
            player["hunting_remaining"] = hunting_flags.get("remain", 0)
            player["hunting_label"] = hunting_progress_label(
                hunting_flags.get("current", 0), hunting_flags.get("select", 1),
                hunting_flags.get("remain", 0), hunting_flags.get("complete", 0),
                language)

            cur.execute("""
                SELECT id, `window`, pos, `count`, vnum, socket0, socket1, socket2,
                       attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2,
                       attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5,
                       attrtype6, attrvalue6
                FROM player.item
                WHERE owner_id = %s
                ORDER BY `window` ASC, pos ASC
            """, (pid,))
            items = cur.fetchall()

            wear_map = {
                0: "body",       # Zbroja
                1: "head",       # Hełm
                2: "foots",      # Buty
                3: "wrist",      # Bransoleta
                4: "weapon",     # Broń
                5: "neck",       # Naszyjnik
                6: "ear",        # Kolczyki
                10: "shield"     # Tarcza (WEAR_SHIELD; slot 7 is WEAR_UNIQUE1)
            }
            equipment = {}
            inventory = []

            for it in items:
                vnum = it.get("vnum") or 0
                name = localized_item_name(vnum, language)
                count = it.get("count") or 1
                pos = it.get("pos") or 0
                win = it.get("window") or ""

                sockets = [it.get("socket0") or 0, it.get("socket1") or 0, it.get("socket2") or 0]
                attrs = []
                for a_idx in range(7):
                    atype = it.get(f"attrtype{a_idx}") or 0
                    aval = it.get(f"attrvalue{a_idx}") or 0
                    if atype != 0 and aval != 0:
                        attrs.append({"type": atype, "val": aval})

                item_obj = {
                    "id": it.get("id"),
                    "vnum": vnum,
                    "name": name,
                    "count": count,
                    "pos": pos,
                    "sockets": sockets,
                    "attrs": attrs
                }

                if win == "EQUIPMENT" and pos in wear_map:
                    equipment[wear_map[pos]] = item_obj
                elif win == "INVENTORY":
                    inventory.append(item_obj)

            return jsonify({
                "ok": True,
                "player": player,
                "equipment": equipment,
                "inventory": inventory
            })
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)})

@app.route("/api/bot_rankings")
def api_bot_rankings():
    rtype = request.args.get("type", "level")
    try:
        rank_limit = max(15, min(100, int(request.args.get("limit", "15"))))
    except (TypeError, ValueError):
        rank_limit = 15
    language = lang()
    messages = map_i18n(language)
    try:
        with db() as c, c.cursor() as cur:
            if rtype == "gold":
                cur.execute("""
                    SELECT id, name, level, job, gold
                    FROM player.player
                    WHERE name LIKE 'bot%%'
                    ORDER BY gold DESC, level DESC
                    LIMIT %s
                """, (rank_limit,))
            elif rtype == "weapon30":
                cur.execute("""
                    SELECT p.id, p.name, p.level, p.job, p.gold,
                           i.vnum as weapon_vnum, i.window as item_window,
                           i.attrtype0, i.attrvalue0, i.attrtype1, i.attrvalue1,
                           i.attrtype2, i.attrvalue2, i.attrtype3, i.attrvalue3,
                           i.attrtype4, i.attrvalue4, i.attrtype5, i.attrvalue5,
                           i.attrtype6, i.attrvalue6
                    FROM player.item i
                    JOIN player.player p ON p.id = i.owner_id
                    WHERE p.name LIKE 'bot%%' AND (
                        (i.vnum BETWEEN 290 AND 299) OR
                        (i.vnum BETWEEN 1170 AND 1179) OR
                        (i.vnum BETWEEN 2150 AND 2159) OR
                        (i.vnum BETWEEN 3210 AND 3219) OR
                        (i.vnum BETWEEN 5110 AND 5119) OR
                        (i.vnum BETWEEN 7160 AND 7169)
                    )
                    ORDER BY i.id DESC
                    LIMIT %s
                """, (rank_limit,))
            elif rtype == "weapon":
                cur.execute("""
                    SELECT p.id, p.name, p.level, p.job, p.gold, i.vnum as weapon_vnum
                    FROM player.player p
                    LEFT JOIN player.item i ON p.id = i.owner_id AND i.window = 'EQUIPMENT' AND i.pos = 4
                    WHERE p.name LIKE 'bot%%'
                    ORDER BY MOD(i.vnum, 10) DESC, i.vnum DESC, p.level DESC
                    LIMIT %s
                """, (rank_limit,))
            elif rtype == "armor":
                cur.execute("""
                    SELECT p.id, p.name, p.level, p.job, p.gold, i.vnum as armor_vnum
                    FROM player.player p
                    LEFT JOIN player.item i ON p.id = i.owner_id AND i.window = 'EQUIPMENT' AND i.pos = 0
                    WHERE p.name LIKE 'bot%%'
                    ORDER BY MOD(i.vnum, 10) DESC, i.vnum DESC, p.level DESC
                    LIMIT %s
                """, (rank_limit,))
            elif rtype == "items":
                cur.execute("""
                    SELECT p.id, p.name, p.level, p.job, p.gold, COUNT(i.id) as item_count
                    FROM player.player p
                    LEFT JOIN player.item i ON p.id = i.owner_id AND i.window = 'INVENTORY'
                    WHERE p.name LIKE 'bot%%'
                    GROUP BY p.id
                    ORDER BY item_count DESC, p.level DESC
                    LIMIT %s
                """, (rank_limit,))
            elif rtype == "horse":
                cur.execute("""
                    SELECT id, name, level, job, gold, horse_level
                    FROM player.player
                    WHERE name LIKE 'bot%%'
                    ORDER BY horse_level DESC, level DESC, exp DESC
                    LIMIT %s
                """, (rank_limit,))
            elif rtype == "biologist":
                mission_names = tuple(m[0] for m in BIOLOGIST_MISSIONS)
                placeholders = ",".join(["%s"] * len(mission_names))
                ranking_sql = """
                    SELECT p.id, p.name, p.level, p.job, p.gold,
                           COUNT(DISTINCT CASE
                               WHEN q.szState = '__status' AND q.lValue = %s
                               THEN q.szName END) AS biologist_completed
                    FROM player.player p
                    LEFT JOIN player.quest q
                      ON q.dwPID = p.id AND q.szName IN ({})
                    WHERE p.name LIKE 'bot%%'
                    GROUP BY p.id
                    ORDER BY biologist_completed DESC, p.level DESC, p.exp DESC
                    LIMIT %s
                """.format(placeholders)
                cur.execute(ranking_sql, (BIOLOGIST_COMPLETE_STATE,) + mission_names +
                            (rank_limit,))
            elif rtype == "hunting":
                cur.execute("""
                    SELECT p.id, p.name, p.level, p.job, p.gold,
                           MAX(CASE WHEN q.szState = 'complete' THEN q.lValue ELSE 0 END) AS hunting_complete,
                           MAX(CASE WHEN q.szState = 'current' THEN q.lValue ELSE 0 END) AS hunting_current,
                           MAX(CASE WHEN q.szState = 'select' THEN q.lValue ELSE 1 END) AS hunting_select,
                           MAX(CASE WHEN q.szState = 'remain' THEN q.lValue ELSE 0 END) AS hunting_remain
                    FROM player.player p
                    LEFT JOIN player.quest q
                      ON q.dwPID = p.id AND q.szName = 'levelup'
                    WHERE p.name LIKE 'bot%%'
                    GROUP BY p.id
                    ORDER BY hunting_complete DESC, hunting_current DESC,
                              hunting_remain ASC, p.level DESC
                    LIMIT %s
                """, (rank_limit,))
            else: # level
                cur.execute("""
                    SELECT id, name, level, job, exp, gold
                    FROM player.player
                    WHERE name LIKE 'bot%%'
                    ORDER BY level DESC, exp DESC
                    LIMIT %s
                """, (rank_limit,))

            rows = cur.fetchall()
            rankings = []
            for r in rows:
                wv = r.get("weapon_vnum")
                av = r.get("armor_vnum")
                w_name = localized_item_name(wv, language) if wv else ""
                a_name = localized_item_name(av, language) if av else ""

                sr = 0
                um = 0
                for a_idx in range(7):
                    atype = r.get(f"attrtype{a_idx}")
                    aval = r.get(f"attrvalue{a_idx}")
                    if atype == 71: sr = int(aval or 0)
                    if atype == 72: um = int(aval or 0)

                bio_completed = max(0, min(len(BIOLOGIST_MISSIONS), int(r.get("biologist_completed") or 0)))
                if bio_completed >= len(BIOLOGIST_MISSIONS):
                    bio_label = "%d/%d • %s" % (
                        bio_completed, len(BIOLOGIST_MISSIONS), messages["bio_complete"])
                elif bio_completed > 0:
                    mission = BIOLOGIST_MISSIONS[bio_completed - 1]
                    bio_label = "%d/%d • %s" % (
                        bio_completed, len(BIOLOGIST_MISSIONS),
                        localized_biologist_name(mission[0], mission[2], language))
                else:
                    bio_label = "0/%d • %s" % (
                        len(BIOLOGIST_MISSIONS), messages["bio_in_progress"])
                hunting_complete = max(0, int(r.get("hunting_complete") or 0))
                hunting_current = max(0, int(r.get("hunting_current") or 0))
                hunting_remain = max(0, int(r.get("hunting_remain") or 0))
                hunting_label = hunting_progress_label(
                    hunting_current, r.get("hunting_select") or 1,
                    hunting_remain, hunting_complete, language)
                rankings.append({
                    "id": r["id"],
                    "name": r["name"],
                    "level": r["level"],
                    "job": localized_job_name(r.get("job", 0), language),
                    "gold": r.get("gold", 0),
                    "weapon_vnum": wv,
                    "weapon_name": w_name,
                    "item_window": r.get("item_window", "INVENTORY"),
                    "sr": sr,
                    "um": um,
                    "armor_name": a_name,
                    "item_count": r.get("item_count", 0),
                    "horse_level": r.get("horse_level", 0),
                    "biologist_completed": bio_completed,
                    "biologist_label": bio_label,
                    "hunting_complete": hunting_complete,
                    "hunting_current": hunting_current,
                    "hunting_remaining": hunting_remain,
                    "hunting_label": hunting_label,
                    "in_pt": bot_in_party_cohort(r["id"])
                })

            if rtype == "weapon30":
                rankings.sort(key=lambda x: (x["sr"], x["um"], x["level"]), reverse=True)

            return jsonify({"ok": True, "type": rtype, "limit": rank_limit,
                            "rankings": rankings})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e), "rankings": []})

@app.route("/rates", methods=["GET", "POST"])
@login_required
def rates():
    """Server-wide experience / item drop / yang rates. Applying them restarts the game."""
    have_script = os.path.exists(RATES_SCRIPT)
    if request.method == "POST":
        # (the global before_request hook has already checked the CSRF token)
        #
        # Three of the things that can go wrong here — no helper, no table, no
        # database — are conditions rather than events: they are still true a
        # moment later, and the GET this redirects to looks for every one of
        # them and says so itself. Saying it here as well is what put the same
        # red box on the page twice. So these three only redirect, and the
        # page below does the talking. Anything that is NOT still true on the
        # next page load (a number out of range, a helper that would not
        # start) is flashed here, because nothing else would ever mention it.
        if not have_script:
            return redirect(url_for("rates"))
        vals = {n: clean_rate(request.form.get(n, "")) for n in RATE_NAMES}
        if any(v is None for v in vals.values()):
            flash(t("rates_range"), "error")
            return redirect(url_for("rates"))
        try:
            with db() as c, c.cursor() as cur:
                for n in RATE_NAMES:
                    cur.execute("INSERT INTO player.web_admin_rates (name,value) VALUES (%s,%s) "
                                "ON DUPLICATE KEY UPDATE value=VALUES(value)", (n, vals[n]))
        except pymysql.err.ProgrammingError:
            return redirect(url_for("rates"))     # "the table is missing" — said below
        except Exception:
            return redirect(url_for("rates"))     # "the database is down"  — said below
        write_rates_status("running")
        try:
            # Never wait for this one: it stops and restarts the whole game server,
            # which takes far longer than a browser is prepared to sit on a request.
            subprocess.Popen(["/bin/sh", RATES_SCRIPT],
                             stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL, close_fds=True,
                             start_new_session=True)
        except Exception:
            flash(t("rates_st_failed"), "error")
            return redirect(url_for("rates"))
        flash(t("rates_saved"))
        return redirect(url_for("rates"))

    cur_rates = {n: 100 for n in RATE_NAMES}
    try:
        cur_rates = read_rates()
    except pymysql.err.ProgrammingError:
        flash(t("rates_no_table"), "error")
    except Exception:
        flash(t("db_down"), "error")
    if not have_script:
        flash(t("rates_no_script"), "error")
    st = rates_status().get("state", "")
    return render_template_string(TPL_RATES, cur=cur_rates, presets=RATE_PRESETS,
                                  state_msg=t("rates_st_" + st) if st in RATE_STATES else "")

# =============================================================================
#  The patch log, and the update page.
# =============================================================================

# The manual update, written out. The same six lines the watcher runs, in the
# same order, so that the button and the keyboard do exactly the same thing --
# and so that an operator who does not want the button is not left guessing.
#
# This is the fallback for a stack somebody assembled by hand. An installer
# knows better: it knows the one command that reinstalls this exact server, and
# it leaves that command here. On Windows that is the whole update story -- the
# installer is idempotent, so running it again pulls the new version, rebuilds
# and restarts, and nothing has to run in the background waiting to do it.
MANUAL_UPDATE = """REPO=/var/cache/m2src/repo
STACK=/opt/metin2/stack

git -C "$REPO" fetch --depth 1 origin main
git -C "$REPO" reset --hard FETCH_HEAD
sh "$REPO/linux-port/fetch-sources.sh" fetch
(cd "$REPO/linux-port/docker" && tar cf - .) | (cd "$STACK" && tar xf -)
cd "$STACK" && docker compose up -d --build"""

def manual_update():
    return str(CONF.get("update_command", "") or "").strip() or MANUAL_UPDATE

TPL_PATCHLOG = BASE.replace("__BODY__", """
<p><a href="{{url_for('dash')}}">{{t('back_players')}}</a></p>

<div class="card">
<h3>📜 {{t('pl_nav')}}</h3>
<p><span class="badge">{{t('ver_label')}} {{ upd.current if upd.current else t('ver_unknown') }}</span>
{% if upd.available %}<span class="badge" style="border-color:var(--gold)">⬆️ {{upd.latest}}</span>{% endif %}</p>
{% if not upd.current %}<p class="muted">{{t('ver_unknown_why')}}</p>{% endif %}
{% if not upd.enabled %}
<p class="muted"><b>{{t('upd_off_t')}}.</b> {{t('upd_off')}}</p>
{% elif upd.available %}
<p class="muted">{{ t('upd_avail').replace('{cur}', upd.current or t('ver_unknown')).replace('{new}', upd.latest) }}</p>
{% elif not upd.checked %}
<p class="muted">{% if upd.error %}{{t('upd_failed')}}{% else %}{{t('upd_never')}}{% endif %}</p>
{% else %}
<p class="muted">{{t('upd_none')}}{% if upd.error %} — {{t('upd_failed')}}{% endif %}</p>
{% endif %}
{# Asks straight away instead of waiting for the daily check -- the one place
   in the panel where a page deliberately waits for the network, because
   somebody pressed a button and is owed an answer. #}
{% if upd.enabled %}
<form method="post" action="{{url_for('patchlog_check')}}" style="margin-top:10px">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<button class="btn">{{t('pl_check')}}</button>
</form>
{% endif %}
{% if upd.checked %}<p class="muted" style="font-size:12px">{{t('upd_checked')}}: {{checked_at}}</p>{% endif %}
</div>

{% if upd.available %}
<div class="card">
<h3>⬆️ {{t('upd_page_t')}}</h3>
{% if can_apply %}
<p class="muted">{{t('upd_warn')}}</p>
<a class="btn" href="{{url_for('update_page')}}">{{t('upd_open')}}</a>
{% elif apply_on %}
<p class="muted"><b>{{t('upd_no_watcher_t')}}.</b> {{t('upd_no_watcher')}}</p>
<pre class="cmd">cd /opt/metin2/stack
docker compose --profile update up -d updater</pre>
{% else %}
<p class="muted">{{t('upd_manual')}}</p>
<pre class="cmd">{{manual}}</pre>
<p class="muted">{{t('upd_manual_keeps')}}</p>
{% endif %}
</div>

{% endif %}

{# Two sections, and nothing appears in both: the changelog is split at the
   version this panel is running. Above the line is what an update would add,
   below it is what is already here. #}
{% if remote %}
<div class="card">
<h3>⬆️ {{t('pl_new_t')}}</h3>
<div class="md">{{remote}}</div>
</div>
{% endif %}

<div class="card">
<h3>{{t('pl_have_t')}}</h3>
{% if local %}<div class="md">{{local}}</div>
{% else %}<p class="muted">{{t('pl_none')}}</p>{% endif %}
</div>

<div class="card about">
<h3>🌐 {{t('upd_phone_t')}}</h3>
<p>{{t('upd_phone')}}</p>
<p>{% if upd.enabled %}{{t('upd_phone_off')}}{% else %}{{t('upd_off')}}{% endif %}</p>
</div>""")

TPL_UPDATE = BASE.replace("__BODY__", """
<p><a href="{{url_for('patchlog')}}">← {{t('pl_nav')}}</a></p>

<div class="card">
<h3>⬆️ {{t('upd_page_t')}}</h3>
<p><span class="badge">{{ upd.current if upd.current else t('ver_unknown') }}</span> →
   <span class="badge" style="border-color:var(--gold)">{{upd.latest}}</span></p>
</div>

{# While one is in flight the "start it" block is gone entirely -- there is
   nothing sensible a second press could do, so there is nothing to press. #}
{% if state not in ('queued', 'running') %}
<div class="card">
<h3>⚠️ {{t('upd_warn_t')}}</h3>
<p class="muted">{{t('upd_warn')}}</p>
<p class="muted">{{t('upd_warn_panel')}}</p>
{% if can_apply %}
<form method="post" action="{{url_for('update_start')}}">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
<button class="big">{{t('upd_start')}}</button></form>
{% else %}
<p class="muted">{{t('upd_not_now')}}</p>
{% endif %}
</div>
{% endif %}

{% if state %}
<div class="card">
<h3>{{t('upd_progress')}}</h3>
<p id="ustate" class="muted">{{state_msg or t('upd_waiting')}}</p>
<pre class="cmd" id="ulog" style="max-height:340px;overflow:auto">{{log}}</pre>
</div>
<script>
(function(){
 var st=document.getElementById('ustate'), lg=document.getElementById('ulog');
 var LOST={{ lost_msg|tojson }};
 function tick(){
  fetch('{{url_for('api_update')}}',{cache:'no-store'})
   .then(function(r){return r.json();})
   .then(function(d){
     // textContent, never innerHTML: this is a log file written by another
     // program and it is shown as the characters it contains, nothing more.
     if(d.message) st.textContent = d.message;
     if(typeof d.log === 'string' && d.log !== lg.textContent){
       var atEnd = lg.scrollTop + lg.clientHeight >= lg.scrollHeight - 24;
       lg.textContent = d.log;
       if(atEnd) lg.scrollTop = lg.scrollHeight;
     }
     if(d.state === 'ok' || d.state === 'failed'){ window.setTimeout(function(){
        window.location.reload(); }, 4000); return; }
     window.setTimeout(tick, 3000);
   })
   .catch(function(){
     // Expected, once: the panel is one of the containers being recreated.
     st.textContent = LOST;
     window.setTimeout(tick, 3000);
   });
 }
 window.setTimeout(tick, 1500);
})();
</script>
{% endif %}""")

UPDATE_STATES = ("queued", "running", "ok", "failed")

def _checked_at(ts):
    if not ts:
        return ""
    try:
        return datetime.datetime.fromtimestamp(ts).strftime("%Y-%m-%d %H:%M")
    except (ValueError, OSError, OverflowError):
        return ""

@app.route("/patchlog")
@login_required
def patchlog():
    """What this build changed, and what a newer one would change.

    Both sides are Markdown and both go through the same renderer, which
    escapes before it formats. The published one arrived over the network; the
    local one came out of the image. Neither is trusted more than the other,
    because neither needs to be.
    """
    # One source when there is one. The published file is cumulative, so it
    # carries both halves: split it at the running version and neither side
    # repeats the other. Without it, all that is known is what shipped in this
    # build, and that is entirely "what you are running".
    pub = update_notes()
    if pub:
        newer, upto = changelog_split(pub, PANEL_VERSION)
    else:
        newer, upto = "", local_changelog()
    return render_template_string(
        TPL_PATCHLOG,
        local=md_to_html(upto),
        remote=md_to_html(newer),
        checked_at=_checked_at(update_state()["checked"]),
        can_apply=update_can_apply(),
        apply_on=UPDATE_APPLY,
        manual=manual_update())

@app.route("/patchlog/check", methods=["POST"])
@login_required
def patchlog_check():
    """Ask now, rather than waiting for the daily check.

    Done on the request thread on purpose: somebody pressed a button and is
    waiting for an answer, so an answer is what they get. It is the only place
    in the panel where a page waits for the network, and it is bounded -- the
    fetch has its own timeout and there are at most two of them.
    """
    # The CSRF token is verified for every POST in csrf_protect(), so there is
    # nothing to check here.
    if not UPDATE_CHECK:
        flash(t("pl_check_off"), "error")
        return redirect(url_for("patchlog"))
    # A button anyone logged in can press should not become a way to hammer
    # somebody else's server. Four a minute is far more than a person needs.
    if rate_limited("updcheck", 4, 60):
        flash(t("pl_check_wait"), "error")
        return redirect(url_for("patchlog"))
    _update_check_now()
    st = update_state()
    if st["error"]:
        flash(t("pl_check_bad"), "error")
    elif st["available"]:
        flash(t("pl_check_new").replace("{new}", st["latest"]))
    else:
        flash(t("pl_check_ok"))
    return redirect(url_for("patchlog"))

@app.route("/update")
@login_required
def update_page():
    st = update_status()
    state = st.get("state", "")
    return render_template_string(
        TPL_UPDATE,
        can_apply=update_can_apply(),
        state=state if state in UPDATE_STATES else "",
        state_msg=t("upd_st_" + state) if state in UPDATE_STATES else "",
        log=update_log_tail(),
        lost_msg=t("upd_lost"))

@app.route("/update/start", methods=["POST"])
@login_required
def update_start():
    """Write the request and get out of the way.

    Everything this can do is in these few lines: it writes a file containing
    an id, a version number and a timestamp into a directory. It cannot say
    what should be run, and the watcher on the other side would not read it if
    it could.
    """
    if not update_can_apply():
        flash(t("upd_not_now"), "error")
        return redirect(url_for("patchlog"))
    if not update_request_write(update_state()["latest"]):
        flash(t("upd_req_failed"), "error")
        return redirect(url_for("patchlog"))
    # Say "queued" here rather than waiting for the watcher to say it: the
    # redirect below lands within milliseconds and the watcher polls every few
    # seconds, so without this the page would open on the *previous* update's
    # result for a moment. The watcher overwrites this the instant it starts.
    update_write_status("queued")
    flash(t("upd_started"))
    return redirect(url_for("update_page"))

@app.route("/api/update")
@login_required
def api_update():
    """Progress, for the page above. Admin-only -- nothing here is public."""
    st = update_status()
    state = st.get("state", "")
    state = state if state in UPDATE_STATES else ""
    return jsonify({"state": state,
                    "message": t("upd_st_" + state) if state else "",
                    "log": update_log_tail()})

@app.route("/login", methods=["GET", "POST"])
def login():
    # On a local install there is nothing to log in to -- see local_open(). The
    # page itself still matters though: it carries registration, the game and
    # the server status. An earlier version redirected it to the admin view,
    # which made all of that unreachable and left no way back, because the
    # logout link is hidden when nobody is logged in. So: render it, minus the
    # passphrase box, with a button through to the admin side.
    if local_open() and request.method == "POST":
        return redirect(url_for("login"))
    ip = request.remote_addr
    if request.method == "POST":
        cnt, lock = FAILS.get(ip, [0, 0])
        if time.time() < lock:
            flash("Too many wrong attempts. Please wait 15 minutes for security. ⏳", "error")
            return render_template_string(TPL_LOGIN, client_ready=os.path.exists(CLIENT_ZIP), client_name=CLIENT_LABEL,
                                  client_url=CLIENT_URL, browser_ready=browser_play_ready(),
                                  play_url=play_url())
        if check_pass(request.form.get("pw", "")):
            FAILS.pop(ip, None)
            session["auth"] = True
            return redirect(url_for("dash"))
        cnt += 1
        FAILS[ip] = [cnt, time.time() + LOCK_SEC if cnt >= MAX_FAIL else 0]
        time.sleep(1.5)
        flash("Wrong passphrase, try again. 🙂", "error")
    return render_template_string(TPL_LOGIN, client_ready=os.path.exists(CLIENT_ZIP), client_name=CLIENT_LABEL,
                                  client_url=CLIENT_URL, browser_ready=browser_play_ready(),
                                  play_url=play_url())

def _dl_quota_take(ip):
    """Spend one download slot, against two ceilings, both over a rolling 24h.

      * DL_MAX per address  -- one person cannot loop the download
      * DL_DAY_MAX in total -- a pool of addresses cannot either

    Returns (allowed, seconds_until_a_slot_frees, scope) where scope is "ip" or
    "all" so the page can say which limit was hit; being told "wait 9 hours"
    without being told it is not about you is quietly infuriating.

    Addresses are stored only as a salted hash: the quota has to recognise an
    address again, not know what it was.
    """
    key = hashlib.sha256((CONF["salt"] + "|" + str(ip)).encode()).hexdigest()
    now = time.time()
    con = sqlite3.connect(DL_DB, timeout=15, isolation_level=None)
    try:
        con.execute("PRAGMA journal_mode=WAL")      # readers never block a writer
        con.execute("CREATE TABLE IF NOT EXISTS dl (ip TEXT NOT NULL, ts REAL NOT NULL)")
        con.execute("CREATE INDEX IF NOT EXISTS dl_ip ON dl (ip, ts)")
        con.execute("CREATE INDEX IF NOT EXISTS dl_ts ON dl (ts)")
        # BEGIN IMMEDIATE takes the write lock up front, so the count and the
        # insert cannot straddle another request doing the same thing. Without
        # it, two simultaneous downloads both read "2 used" and both proceed --
        # which is exactly the case a rate limit exists for.
        con.execute("BEGIN IMMEDIATE")
        try:
            con.execute("DELETE FROM dl WHERE ts < ?", (now - DL_WINDOW,))
            total = con.execute("SELECT COUNT(*) FROM dl").fetchone()[0]
            if total >= DL_DAY_MAX:
                oldest = con.execute("SELECT MIN(ts) FROM dl").fetchone()[0] or now
                con.execute("COMMIT")               # keep the cleanup
                return False, max(1, int(oldest + DL_WINDOW - now)), "all"
            rows = con.execute("SELECT ts FROM dl WHERE ip = ? ORDER BY ts",
                               (key,)).fetchall()
            if len(rows) >= DL_MAX:
                con.execute("COMMIT")
                return False, max(1, int(rows[0][0] + DL_WINDOW - now)), "ip"
            con.execute("INSERT INTO dl VALUES (?, ?)", (key, now))
            con.execute("COMMIT")
            return True, 0, ""
        except Exception:
            con.execute("ROLLBACK")
            raise
    finally:
        con.close()

# ---- password reset links ---------------------------------------------------
# There is no self-service "forgot password": the player writes to the admin,
# the admin makes a link here, the player sets a new password through it. Only
# a hash of the token is stored, links are single-use, live 24 hours, and a
# new link for the same account replaces any older one.
def _pwreset_table(con):
    con.execute("CREATE TABLE IF NOT EXISTS pw_reset (th TEXT PRIMARY KEY, "
                "login TEXT NOT NULL, exp REAL NOT NULL)")
    con.execute("DELETE FROM pw_reset WHERE exp < ?", (time.time(),))

def _pwreset_new(login):
    """Make a fresh token for this account, cancelling any earlier one."""
    tok = secrets.token_urlsafe(32)
    th = hashlib.sha256(tok.encode()).hexdigest()
    con = sqlite3.connect(DL_DB, timeout=5)
    try:
        _pwreset_table(con)
        con.execute("DELETE FROM pw_reset WHERE login = ?", (login,))
        con.execute("INSERT INTO pw_reset VALUES (?, ?, ?)", (th, login, time.time() + 24 * 3600))
        con.commit()
        return tok
    finally:
        con.close()

def _pwreset_login_for(tok, burn=False):
    """The account a token belongs to, or None. burn=True spends the token."""
    th = hashlib.sha256(str(tok).encode()).hexdigest()
    con = sqlite3.connect(DL_DB, timeout=5)
    try:
        _pwreset_table(con)
        row = con.execute("SELECT login FROM pw_reset WHERE th = ?", (th,)).fetchone()
        if row and burn:
            con.execute("DELETE FROM pw_reset WHERE th = ?", (th,))
        con.commit()
        return row[0] if row else None
    finally:
        con.close()

@app.route("/admin/resetlink", methods=["POST"])
@login_required
def admin_resetlink():
    lg = request.form.get("login", "").strip()
    if not (4 <= len(lg) <= 16 and lg.isalnum()):
        flash(t("reset_noacc"), "error")
        return redirect(url_for("dash"))
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT 1 FROM account.account WHERE login=%s", (lg,))
            if not cur.fetchone():
                flash(t("reset_noacc"), "error")
                return redirect(url_for("dash"))
    except Exception:
        flash(t("db_down"), "error")
        return redirect(url_for("dash"))
    link = url_for("reset", token=_pwreset_new(lg), _external=True)
    flash("%s %s" % (t("reset_made"), link))
    return redirect(url_for("dash"))

@app.route("/reset/<token>", methods=["GET", "POST"])
def reset(token):
    lg = _pwreset_login_for(token)
    if not lg:
        return render_template_string(TPL_RESET, valid=False, login="")
    if request.method == "POST":
        if rate_limited("pwreset", 5, 900):
            flash("Too many attempts. Please wait a while. ⏳", "error")
            return render_template_string(TPL_RESET, valid=True, login=lg)
        new, new2 = request.form.get("new", ""), request.form.get("new2", "")
        if len(new) < 6:
            flash(t("reset_short"), "error")
        elif new != new2:
            flash(t("reset_mismatch"), "error")
        else:
            try:
                with db() as c, c.cursor() as cur:
                    cur.execute("UPDATE account.account SET password=%s WHERE login=%s",
                                (m2_hash(new), lg))
                _pwreset_login_for(token, burn=True)
                flash(t("reset_done"))
                return redirect(url_for("login"))
            except Exception:
                flash(t("db_down"), "error")
    return render_template_string(TPL_RESET, valid=True, login=lg)

@app.route("/download")
def download():
    """Public client download (no passphrase needed)."""
    # When an external download URL is configured, send people there instead of
    # streaming a gigabyte through this dev server. Anyone with an old bookmark
    # pointing at /download still ends up in the right place.
    if CLIENT_URL:
        return redirect(CLIENT_URL)
    if not os.path.exists(CLIENT_ZIP):
        flash("The game download is not ready yet.", "error")
        return redirect(url_for("login"))
    # A slot is spent only by a fresh fetch of the whole file. HEAD probes cost
    # nothing, and neither does resuming: a genuine resume asks for a Range that
    # starts mid-file. A Range starting at byte 0 is the whole file wearing a
    # different hat, so it pays like one. The logged-in admin is never limited.
    rng = request.headers.get("Range", "")
    fresh = request.method == "GET" and (not rng or bool(re.match(r"\s*bytes\s*=\s*0\s*-", rng)))
    if fresh and not (session.get("auth") or local_open()):
        try:
            allowed, wait, scope = _dl_quota_take(request.remote_addr)
        except Exception:
            # The counter is a guard, not the point of the page. If its little
            # database is unwritable we log it and serve -- refusing every
            # download because a quota file is broken is the worse failure.
            app.logger.exception("download quota unavailable, serving anyway")
            allowed, wait, scope = True, 0, ""
        if not allowed:
            resp = app.response_class(
                render_template_string(TPL_DL_LIMIT,
                                       wait_h=max(1, -(-wait // 3600)),
                                       scope=scope),
                status=429)
            resp.headers["Retry-After"] = str(wait)
            return resp
    # Behind nginx, hand the file over and let it do the sending: a gigabyte
    # through Flask's single-threaded development server blocks the panel for
    # everyone else for as long as the download runs. The header is only obeyed
    # by nginx, so it is set solely for requests that actually arrived through it.
    if behind_proxy():
        resp = app.response_class()
        resp.headers["X-Accel-Redirect"] = "/_client_zip"
        resp.headers["Content-Type"] = "application/zip"
        resp.headers["Content-Disposition"] = 'attachment; filename="%s"' % CLIENT_FILE
        return resp
    return send_file(CLIENT_ZIP, as_attachment=True, download_name=CLIENT_FILE, conditional=True)

# ---------------- The client that runs in a browser ----------------
# Serving it is the panel's job for one reason: same origin. The page, its
# WebAssembly and -- behind nginx -- the bridge then sit under one address, one
# certificate and one port, so nothing has to be opened, allowed or certified a
# second time, and a page served over HTTPS can open the wss:// connection it
# is obliged to use.
#
# It is ~1.7 GB in 421 files: index.html, index.js, a 14.7 MB index.wasm, a
# 3.1 MB manifest.bin and 408 content-addressed blobs of about 4 MB each. That
# is far too much to push through this process -- see the nginx location block
# install.sh writes, which serves the same directory directly and leaves this
# route as the fallback for an install with no proxy in front.

def _play_dir_file(rel):
    """An absolute path inside the browser client's directory, or None if it
    escapes it. realpath on both sides, so `current' being a symlink to the
    versioned directory resolves to the same root the file does."""
    root = os.path.realpath(browser_root())
    full = os.path.realpath(os.path.join(root, rel))
    if full != root and not full.startswith(root + os.sep):
        return None
    return full

def _play_headers(resp, cache):
    """The three caching rules the client's own static server documents.

    Taken from dist/browser/serve-webfs.py rather than invented, because they
    are not a preference -- getting them wrong is not slow, it is broken:

      <hash>.bin   immutable, a year. The name IS the hash of the contents, so
                   the URL can never mean anything else and a second visit is
                   zero network for everything that did not change.
      manifest.bin no-store. It is the mutable root; a cached one makes a
                   patched client load the previous corpus, which presents as
                   missing files rather than as a stale cache.
      index.*      no-cache -- "ask first", so an unchanged build is a 304
                   rather than a 14 MB download.

    Cross-origin isolation headers are deliberately NOT set. They would be
    required for SharedArrayBuffer, and this build has none: the string does
    not occur in index.js or index.wasm, and its pre.js disables JSPI on
    purpose. Sending them anyway would only risk breaking the gate's
    cross-origin probe of the bridge.
    """
    resp.headers["Cache-Control"] = cache
    # The gate fetches /ping from the bridge, which is a different origin when
    # the panel has no certificate. This costs nothing here -- everything under
    # /play is public anyway, and it is what their own server sends.
    resp.headers["Access-Control-Allow-Origin"] = "*"
    return resp

def _play_cache_for(name):
    base = name.rsplit("/", 1)[-1]
    if base in ("manifest.bin", "index.dev"):
        return "no-store"
    if base.endswith(".bin"):
        return "public, max-age=31536000, immutable"
    return "no-cache"

@app.route("/play")
def play_redirect():
    # To /play/ with the slash: every asset the page asks for is relative, so
    # without it the browser resolves them against / and the client fetches 408
    # blobs from the panel's root. A redirect rather than serving here.
    return redirect(play_url() if browser_client_ready() else url_for("login"))

@app.route("/play/")
def play():
    if not browser_client_ready():
        flash("Playing in the browser is not set up on this server.", "error")
        return redirect(url_for("login"))
    resp = send_from_directory(browser_root(), "index.html")
    return _play_headers(resp, "no-cache")

@app.route("/play/<path:sub>")
def play_asset(sub):
    if not browser_client_ready():
        return ("not found", 404)
    if _play_dir_file(sub) is None:
        return ("not found", 404)
    try:
        resp = send_from_directory(browser_root(), sub, conditional=True)
    except Exception:
        return ("not found", 404)
    # Flask guesses .wasm correctly on a modern mimetypes database and not on
    # every one; a wrong type here means the browser refuses to stream-compile
    # it, which looks like a broken client rather than a missing header.
    if sub.endswith(".wasm"):
        resp.headers["Content-Type"] = "application/wasm"
    return _play_headers(resp, _play_cache_for(sub))

# ---------------- Crash reports from the browser client ----------------
#
# The browser client fails where nobody can look. When the wasm module traps,
# the player sees a frozen screen and the trace that names the function which
# called into nothing sits in a console they will never open. Every bug in this
# port has so far cost a round of "please press F12 and paste what it says",
# which only works for people who can reproduce it themselves.
#
# crash-report.js offers to send it. This is where it lands.
#
# The endpoint takes no login on purpose: a player whose client has just died is
# not signed in, and requiring it would lose exactly the reports worth having.
# So it is written to be dull -- a small ceiling, a per-address rate limit, a
# fixed set of fields each with its own length, and a directory that prunes
# itself. Nothing stored here is ever rendered as HTML.
#
# The IP address is NOT stored. The dialog tells the player no personal
# information is sent, an address is personal information, and a promise that is
# only kept in the client is not kept at all. It is used for the rate limit,
# in memory, and then it is gone.
CRASH_DIR = _env_path("M2PANEL_CRASH_DIR", os.path.join(PANEL_DIR, "crash-reports"))
# 64 KB, because a report carries the tail of the client's console log as well
# as the trace. That log is what turns "it crashed" into a line number: the
# client prints PYEXC when a script raises, and that one line has already
# resolved a bug that a stack trace alone pointed at the wrong layer for.
CRASH_MAX_BYTES = 64 * 1024
CRASH_KEEP = 300

def _crash_field(value, limit):
    """One field, as a string, no longer than it is allowed to be."""
    if not isinstance(value, str):
        value = "" if value is None else str(value)
    return value.replace("\r\n", "\n")[:limit]

@app.route("/crash-report", methods=["POST"])
def crash_report():
    if rate_limited("crash", 6, 3600):
        return jsonify(ok=False, error="too many reports from this address"), 429

    raw = request.get_data(cache=False, as_text=False) or b""
    if len(raw) > CRASH_MAX_BYTES:
        return jsonify(ok=False, error="report too large"), 413
    try:
        sent = json.loads(raw.decode("utf-8", "replace"))
        if not isinstance(sent, dict):
            raise ValueError("not an object")
    except Exception:
        return jsonify(ok=False, error="not a JSON object"), 400

    report = {
        "received":    time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "when":        _crash_field(sent.get("when"), 40),
        "message":     _crash_field(sent.get("message"), 2000),
        "stack":       _crash_field(sent.get("stack"), 8000),
        "description": _crash_field(sent.get("description"), 4000),
        # The client's own account of a script error, and the console tail
        # around it. Kept as separate fields because the first is the answer
        # and the second is the context -- reading them apart is the point.
        "pyexc":       _crash_field(sent.get("pyexc"), 8000),
        "log":         _crash_field(sent.get("log"), 16000),
        # The character, so a report can be matched against the game's own logs
        # for the same minute. The client publishes it deliberately and the
        # dialog lists it; the ACCOUNT name still never leaves the wasm heap.
        "character":   _crash_field(sent.get("character"), 40),
        "userAgent":   _crash_field(sent.get("userAgent"), 400),
        "page":        _crash_field(sent.get("page"), 400),
        "serverHost":  _crash_field(sent.get("serverHost"), 120),
        "serverPort":  _crash_field(sent.get("serverPort"), 10),
        "versions":    _crash_field(json.dumps(sent.get("versions") or {}), 400),
    }

    try:
        os.makedirs(CRASH_DIR, exist_ok=True)
        name = "%s-%s.json" % (time.strftime("%Y%m%d-%H%M%S", time.gmtime()),
                               secrets.token_hex(3))
        with open(os.path.join(CRASH_DIR, name), "w", encoding="utf-8") as fh:
            json.dump(report, fh, indent=2, ensure_ascii=False)
        # Keep the newest and no more: an endpoint anybody can post to must not
        # be able to fill the disk, however slowly.
        kept = sorted(f for f in os.listdir(CRASH_DIR) if f.endswith(".json"))
        for old in kept[:-CRASH_KEEP]:
            try:
                os.remove(os.path.join(CRASH_DIR, old))
            except OSError:
                pass
    except Exception as exc:
        app.logger.warning("crash report could not be stored: %s", exc)
        return jsonify(ok=False, error="could not store the report"), 500

    return jsonify(ok=True)

@app.route("/admin/crashes")
@login_required
def crash_list():
    """The reports, newest first, as JSON. Read with a browser or with curl."""
    out = []
    try:
        names = sorted((f for f in os.listdir(CRASH_DIR) if f.endswith(".json")),
                       reverse=True)[:100]
        for name in names:
            try:
                with open(os.path.join(CRASH_DIR, name), encoding="utf-8") as fh:
                    out.append(json.load(fh))
            except Exception:
                continue
    except OSError:
        pass
    return jsonify(count=len(out), reports=out)

# ---------------- Player registration & account ----------------
@app.route("/register", methods=["GET", "POST"])
def register():
    form = {"login": "", "social": ""}
    if request.method == "POST":
        if rate_limited("register", 3, 3600):
            flash("Too many accounts were created from this connection. Please try again later. ⏳", "error")
            return render_template_string(TPL_REGISTER, form=form)
        lg = request.form.get("login", "").strip()
        pw = request.form.get("pw", "")
        pw2 = request.form.get("pw2", "")
        social = request.form.get("social", "").strip()
        form = {"login": lg, "social": social}
        if not (4 <= len(lg) <= 16 and lg.isalnum()):
            flash("The username must be 4-16 letters/numbers, no spaces. 🙂", "error")
        elif len(pw) < 6:
            flash("The password must be at least 6 characters. 🙂", "error")
        elif pw != pw2:
            flash("The two passwords don't match — try again. 🙂", "error")
        elif not (social.isdigit() and len(social) == 7):
            flash("The delete code must be exactly 7 digits (e.g. 1234567). 🙂", "error")
        else:
            try:
                with db() as c, c.cursor() as cur:
                    cur.execute("SELECT 1 FROM account.account WHERE login=%s", (lg,))
                    if cur.fetchone():
                        flash("That username is already taken — pick another one. 🙂", "error")
                        return render_template_string(TPL_REGISTER, form=form)
                    cur.execute(
                        "INSERT INTO account.account (login,password,social_id,status) "
                        "VALUES (%s,%s,%s,'OK')",
                        (lg, m2_hash(pw), social))
                return render_template_string(TPL_REG_DONE,
                                              client_ready=os.path.exists(CLIENT_ZIP),
                                              client_url=CLIENT_URL,
                                              browser_ready=browser_play_ready(),
                                              play_url=play_url())
            except Exception:
                flash("The account could not be created right now. Please try again in a bit. 🙏", "error")
    return render_template_string(TPL_REGISTER, form=form)

@app.route("/account", methods=["GET", "POST"])
def account():
    if request.method == "POST":
        if rate_limited("acclogin", 8, 900):
            flash("Too many attempts. Please wait a while. ⏳", "error")
            return render_template_string(TPL_ACCOUNT_LOGIN)
        lg = request.form.get("login", "").strip()
        pw = request.form.get("pw", "")
        try:
            with db() as c, c.cursor() as cur:
                cur.execute("SELECT password FROM account.account WHERE login=%s", (lg,))
                row = cur.fetchone()
        except Exception:
            flash(t("db_down"), "error")
            return render_template_string(TPL_ACCOUNT_LOGIN)
        if row and hmac.compare_digest(row["password"], m2_hash(pw)):
            session["player"] = lg
        else:
            time.sleep(1.0)
            flash("Wrong username or password. 🙂", "error")
            return render_template_string(TPL_ACCOUNT_LOGIN)
    lg = session.get("player")
    if not lg:
        return render_template_string(TPL_ACCOUNT_LOGIN)
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT id FROM account.account WHERE login=%s", (lg,))
            acc = cur.fetchone()
            chars = []
            if acc:
                cur.execute("SELECT name,job,level,gold FROM player.player WHERE account_id=%s", (acc["id"],))
                chars = cur.fetchall()
    except Exception:
        flash(t("db_down"), "error")
        chars = []
    return render_template_string(TPL_ACCOUNT, login=lg, chars=chars,
                                  emoji=lambda j: JOB_EMOJI.get(j, "🧑"))

@app.route("/account/password", methods=["POST"])
def account_password():
    lg = session.get("player")
    if not lg:
        return redirect(url_for("account"))
    old, new, new2 = (request.form.get(k, "") for k in ("old", "new", "new2"))
    if len(new) < 6:
        flash("The new password must be at least 6 characters. 🙂", "error")
    elif new != new2:
        flash("The two new passwords don't match. 🙂", "error")
    else:
        try:
            with db() as c, c.cursor() as cur:
                cur.execute("SELECT password FROM account.account WHERE login=%s", (lg,))
                row = cur.fetchone()
                if row and hmac.compare_digest(row["password"], m2_hash(old)):
                    cur.execute("UPDATE account.account SET password=%s WHERE login=%s", (m2_hash(new), lg))
                    flash("🔒 Your password was changed! Use the new one next time you log into the game.")
                else:
                    flash("The current password is wrong. 🙂", "error")
        except Exception:
            flash(t("db_down"), "error")
    return redirect(url_for("account"))

@app.route("/account/logout")
def account_logout():
    session.pop("player", None)
    return redirect(url_for("login"))

@app.route("/logout")
def logout():
    session.clear()
    return redirect(url_for("login"))

@app.route("/")
@app.route("/admin")
@login_required
def dash():
    # A local install has no login step, so "/" would drop the owner straight
    # into the admin view with no way to reach the front page -- where the game,
    # registration and the server status live. There, "/" is the front page and
    # the admin view has its own address. Everywhere else this changes nothing:
    # "/" stays the dashboard you reach by entering the passphrase.
    if local_open() and request.path == "/":
        return redirect(url_for("login"))
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT p.id, p.name, p.job, p.level, p.gold, p.last_play, "
                        "a.login AS account "
                        "FROM player.player p "
                        "LEFT JOIN account.account a ON a.id = p.account_id "
                        "ORDER BY p.last_play DESC LIMIT 200")
            players = cur.fetchall()
        # 'recently in the game' marker: last_play within the last 10 minutes.
        # The game stamps it at login/logout, so this is honest about what it
        # knows - the tooltip says 'was in the game', not 'is online'.
        now = datetime.datetime.now()
        for p in players:
            lp = p.get("last_play")
            # A character who has never played carries MySQL's zero date,
            # '0000-00-00 00:00:00'. There is no such moment, so the driver
            # cannot build a datetime out of it and hands back the raw string
            # instead -- and subtracting a string from a datetime raised
            # TypeError, which this function's own except clause then reported
            # as "the database cannot be reached". A brand-new server, where
            # somebody has registered but not yet logged in, showed a database
            # error on every visit and sent people looking in the wrong place.
            if not isinstance(lp, datetime.datetime):
                lp = None
            p["active"] = bool(lp) and abs((now - lp).total_seconds()) < 600
    except Exception:
        # Log it. Everything here is reported to the visitor as "database
        # unreachable", which is a guess -- and when the guess is wrong it
        # costs an hour of looking at a database that was never broken.
        app.logger.exception("dashboard query failed")
        flash(t("db_down"), "error")
        players = []
    return render_template_string(TPL_DASH, players=players,
                                  emoji=lambda j: JOB_EMOJI.get(j, "🧑"),
                                  jobname=lambda j: JOB_NAME.get(j, ""))

@app.route("/player/<int:pid>")
@login_required
def player(pid):
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT * FROM player.player WHERE id=%s", (pid,))
            p = cur.fetchone()
    except Exception:
        flash(t("db_down"), "error")
        return render_template_string(TPL_DASH, players=[],
                                      emoji=lambda j: JOB_EMOJI.get(j, "🧑"),
                                      jobname=lambda j: JOB_NAME.get(j, ""))
    if not p:
        flash(t("not_found"), "error")
        return redirect(url_for("dash"))
    # read-only inventory: names resolved from items.json, unknown vnums shown
    # as #vnum rather than hidden - the admin should see everything
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT vnum, count, window FROM player.item "
                        "WHERE owner_id=%s ORDER BY window, pos", (pid,))
            inv = [{"name": ITEM_NAMES.get(r["vnum"], "#%d" % r["vnum"]),
                    "count": r["count"], "window": str(r["window"]).lower()}
                   for r in cur.fetchall()]
    except Exception:
        inv = None
    return render_template_string(TPL_PLAYER, p=p, inv=inv,
                                  emoji=lambda j: JOB_EMOJI.get(j, "🧑"),
                                  cats=CATS,
                                  gold_presets=gold_presets_i18n(), warp_presets=warp_presets_i18n(),
                                  speed_presets=speed_presets_i18n(),
                                  gm_ranks=gm_ranks_i18n(), gm_rank=gm_rank_of(p["name"]),
                                  gm_rank_label=gm_rank_label)

def gm_rank_of(name):
    """The rank this character holds in common.gmlist, or None for a normal
    player. Also None when the table cannot be read: the card then just offers
    the choice, which is a better failure than claiming somebody is nobody."""
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT mAuthority AS a FROM common.gmlist WHERE mName=%s LIMIT 1",
                        (name,))
            row = cur.fetchone()
    except Exception:
        return None
    if not row:
        return None
    # PLAYER is in the ENUM and means "no rank": the server skips those rows
    # when it reads the list. Anything else unrecognised is treated the same.
    rank = (row["a"] or "").strip()
    return rank if rank in GM_RANK_SET else None

def item_qty(raw):
    """Return a stack size the game can actually store (1..65535), or None if invalid."""
    try:
        q = int(str(raw).strip() or "1")
    except (TypeError, ValueError):
        return None
    return q if 1 <= q <= MAX_ITEM_COUNT else None

def queue_and_wait(name, cmd, arg1, arg2, wait=7.0):
    """Insert the command into the queue and wait for the in-game quest to process it.
       Returns (status, queue_row_id) with status: done | player_offline | timeout | gone | ...

       IMPORTANT: on 'timeout' the row is still 'pending', so the quest may still pick it
       up later. The caller MUST claim the row (see action()) before applying anything
       itself, otherwise the player would receive the same reward twice."""
    with db() as c, c.cursor() as cur:
        cur.execute("INSERT INTO player.web_admin_queue (player_name,cmd,arg1,arg2) VALUES (%s,%s,%s,%s)",
                    (name, cmd, str(arg1), str(arg2)))
        qid = cur.lastrowid
    deadline = time.time() + wait
    while time.time() < deadline:
        time.sleep(0.6)
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT status FROM player.web_admin_queue WHERE id=%s", (qid,))
            row = cur.fetchone()
        if row is None:
            return "gone", qid          # something removed the row — never apply on top of that
        if row["status"] != "pending":
            return row["status"], qid
    return "timeout", qid

def offline_apply(cur, pid, cmd, arg1, arg2, reason="timeout", name=""):
    """Write the change straight into the database because the player isn't in game.
       Only ITEM / GOLD / LEVEL can be done this way. The caller must have claimed the
       queue row first, otherwise the quest could apply the same thing a second time."""
    if cmd == "GOLD":
        cur.execute("UPDATE player.player SET gold=GREATEST(0,gold+%s) WHERE id=%s", (int(arg1), pid))
    elif cmd == "LEVEL":
        cur.execute("UPDATE player.player SET level=%s WHERE id=%s", (int(arg1), pid))
    elif cmd == "ITEM":
        qty = item_qty(arg2)
        if qty is None:
            raise RuntimeError(t("qty_range").format(max="{:,}".format(MAX_ITEM_COUNT)))
        cur.execute("SELECT pos FROM player.item WHERE owner_id=%s AND window='INVENTORY'", (pid,))
        used = {r["pos"] for r in cur.fetchall()}
        free = next((i for i in range(INVENTORY_SLOTS) if i not in used), None)
        if free is None:
            raise RuntimeError(t("inv_full").format(conf=CONF_PATH))
        cur.execute("INSERT INTO player.item (owner_id,window,pos,count,vnum) VALUES (%s,'INVENTORY',%s,%s,%s)",
                    (pid, free, qty, int(arg1)))
    else:
        # WARP / SPEED need the in-game quest. Faking a warp by writing x/y/map_index is
        # NOT safe (map_index can't be derived from coordinates — the character could end
        # up in the void), so we refuse honestly and say why.
        if reason == "player_offline":
            key = "ingame_offline"
        elif not ingame_helper_seen():
            # Nothing has ever answered here, so this is not a timeout that
            # might go the other way next time -- it is the build. Say that,
            # instead of suggesting they check whether the game is running.
            key = "ingame_nohelper"
        else:
            key = "ingame_timeout"
        raise RuntimeError(t(key).format(name=name))

@app.route("/action", methods=["POST"])
@login_required
def action():
    try:
        pid = int(request.form.get("pid", ""))
    except (TypeError, ValueError):
        flash(t("not_found"), "error")
        return redirect(url_for("dash"))
    cmd = request.form.get("cmd", "")
    preset = request.form.get("preset", "")
    arg1 = request.form.get("arg1", "").strip()
    arg2 = request.form.get("arg2", "1").strip()

    # resolve presets
    if cmd == "ITEM":
        arg1 = request.form.get("custom_vnum", "").strip()  # vnum chosen via live search
        if item_qty(arg2) is None:                          # player.item.count is smallint unsigned
            flash(t("qty_range").format(max="{:,}".format(MAX_ITEM_COUNT)), "error")
            return redirect(url_for("player", pid=pid))
        arg2 = str(item_qty(arg2))
    elif cmd == "GOLD":
        arg1 = request.form.get("custom_amt", "").strip() if preset == "custom" else preset
    elif cmd == "WARP":
        if " " not in preset:
            flash(t("act_novalue"), "error")
            return redirect(url_for("player", pid=pid))
        arg1, arg2 = preset.split(" ", 1)
    elif cmd == "SPEED":
        arg2 = "3600"
    elif cmd == "LEVEL":
        # Checked here rather than left to the server, which does not refuse it
        # -- it returns from PointChange without a word and reports success all
        # the way back up. See the note above MAX_LEVEL.
        try:
            _lv = int(str(arg1).strip())
        except (TypeError, ValueError):
            _lv = None
        if _lv is None or not (1 <= _lv <= MAX_LEVEL):
            flash(t("level_range").format(max=MAX_LEVEL), "error")
            return redirect(url_for("player", pid=pid))
        arg1 = str(_lv)

    if not arg1:
        flash(t("act_novalue"), "error")
        return redirect(url_for("player", pid=pid))

    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT name FROM player.player WHERE id=%s", (pid,))
            row = cur.fetchone()
    except Exception:
        flash(t("db_down"), "error")
        return redirect(url_for("dash"))
    if not row:
        flash(t("not_found"), "error")
        return redirect(url_for("dash"))
    name = row["name"]

    try:
        st, qid = queue_and_wait(name, cmd, arg1, arg2)
        if st == "done":
            flash(t("act_done").format(name=name))
        elif st in ("player_offline", "timeout", "gone"):
            with db() as c, c.cursor() as cur:
                # Take the queue row away from the in-game quest FIRST. If we skipped this
                # and the quest picked the still-pending row up afterwards, the player
                # would get the item/yang/level a second time.
                cur.execute("UPDATE player.web_admin_queue SET status='cancelled' "
                            "WHERE id=%s AND status='pending'", (qid,))
                claimed = cur.rowcount == 1
                if not claimed and st == "player_offline":
                    # The quest itself reported the player as offline, so the row is
                    # already out of the 'pending' pool and nobody will deliver it.
                    claimed = True
                if claimed:
                    offline_apply(cur, pid, cmd, arg1, arg2, reason=st, name=name)
                    # "They weren't in game" is only true when the quest said so.
                    # On a build with no quest at all it is a guess, and a wrong
                    # one for anybody who is playing while they read it.
                    if st == "timeout" and not ingame_helper_seen():
                        flash(t("act_nohelper").format(name=name))
                    else:
                        flash(t("act_offline").format(name=name))
                else:
                    # The quest grabbed it while we were waiting — do NOT apply again.
                    cur.execute("SELECT status FROM player.web_admin_queue WHERE id=%s", (qid,))
                    r2 = cur.fetchone()
                    st2 = r2["status"] if r2 else "gone"
                    if st2 == "done":
                        flash(t("act_late_done").format(name=name))
                    else:
                        flash(t("act_late_other").format(name=name, status=st2))
        else:
            flash(t("act_error").format(status=st), "error")
    except RuntimeError as e:
        flash(str(e), "error")
    except Exception:
        flash(t("act_unexpected"), "error")
    return redirect(url_for("player", pid=pid))

@app.route("/language", methods=["POST"])
@login_required
def set_language():
    """Change the language the game speaks.

    The panel only asks. m2-lang in the game container puts the four files in
    place and the supervisor restarts the cores, because they read those files
    while they boot and never again -- so this disconnects anyone playing for
    the same half minute a rate change does.

    The client is the other half and this does not touch it: its menus and item
    names come from a pack chosen by the locale.cfg next to the .exe. New
    downloads are built to match; anyone who already has the game renames one
    file, which is what the page says after a switch.
    """
    code = (request.form.get("lang", "") or "").strip().lower()
    if code not in GAME_LANG_NAMES:
        flash(t("act_novalue"), "error")
        return redirect(url_for("dash"))

    if code == game_lang():
        flash(t("lang_same").format(lang=GAME_LANG_NAMES[code]))
        return redirect(url_for("dash"))

    if not lang_ask_for(code):
        flash(t("lang_nospool"), "error")
        return redirect(url_for("dash"))

    # Said here rather than after the fact: the game container picks the request
    # up within five seconds and then restarts the cores, so by the time this
    # page comes back the switch is under way but not finished.
    flash(t("lang_asked").format(lang=GAME_LANG_NAMES[code]))
    return redirect(url_for("dash"))

@app.route("/passphrase", methods=["POST"])
@login_required
def set_passphrase():
    """Choose a new admin passphrase.

    Writes the new salt and hash into m2panel.conf and the plaintext into
    PASSPHRASE_FILE, so the installer can go on printing the passphrase at the
    end of every run -- see the note above PASSPHRASE_FILE for why that file
    exists at all.

    The session survives: the Flask secret is not touched, so the operator is
    not thrown out of the page they are standing on.
    """
    # If the hash came from the environment, the file is not what the panel
    # reads and rewriting it would look like it worked until the next restart.
    # Only a hand-assembled stack does this; ours passes neither.
    if os.environ.get("M2PANEL_SALT", "").strip() or os.environ.get("M2PANEL_PASS_HASH", "").strip():
        flash(t("pp_env"), "error")
        return redirect(url_for("dash"))

    new  = request.form.get("new", "")
    new2 = request.form.get("new2", "")

    # On a local install there is no passphrase in use -- login_required lets
    # everybody through, and asking for a current one nobody has ever typed
    # would make the form impossible to submit. Everywhere else, holding a
    # session is not enough to change the password on it.
    if not local_open():
        if not check_pass(request.form.get("old", "")):
            flash(t("pp_bad_old"), "error")
            return redirect(url_for("dash"))

    if len(new) < PASSPHRASE_MIN:
        flash(t("pp_short").format(n=PASSPHRASE_MIN), "error")
        return redirect(url_for("dash"))
    if new != new2:
        flash(t("pp_mismatch"), "error")
        return redirect(url_for("dash"))

    salt = secrets.token_hex(16)
    ph   = hashlib.pbkdf2_hmac("sha256", new.encode(), salt.encode(), 200_000).hex()

    try:
        # Read, change two keys, write back. Everything else in the file is the
        # entrypoint's and none of it is ours to rewrite.
        with open(CONF_PATH, encoding="utf-8") as f:
            conf_on_disk = json.load(f)
        conf_on_disk["salt"] = salt
        conf_on_disk["pass_hash"] = ph

        # Written beside the real file and moved over it, so a crash halfway
        # through cannot leave a config that locks the operator out of their own
        # panel with no way back in.
        tmp = CONF_PATH + ".new"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(conf_on_disk, f, indent=2)
        os.chmod(tmp, 0o600)
        os.replace(tmp, CONF_PATH)
    except Exception:
        flash(t("pp_failed"), "error")
        return redirect(url_for("dash"))

    # Live, for this process, before anything can ask again.
    CONF["salt"] = salt
    CONF["pass_hash"] = ph

    # For the installer. A failure here costs the operator nothing they can see
    # now -- the new passphrase works either way -- so it must not read as if
    # the change did not happen.
    kept = True
    try:
        tmp = PASSPHRASE_FILE + ".new"
        with open(tmp, "w", encoding="utf-8") as f:
            f.write(new + "\n")
        os.chmod(tmp, 0o600)
        os.replace(tmp, PASSPHRASE_FILE)
    except OSError:
        kept = False

    session["auth"] = True
    flash(t("pp_done") if kept else t("pp_done_unsaved"))
    return redirect(url_for("dash"))

@app.route("/gm", methods=["POST"])
@login_required
def set_gm():
    """Give a character the in-game admin commands, or take them away.

    Unlike everything on the action page this does not go through the in-game
    helper, and it does not need the player to be online: the game reads its
    list of game masters from common.gmlist, so the row IS the grant. What the
    helper cannot do is make the running server notice, which is what the
    reload request at the bottom is for.

    Two details of the server's own matching decide the shape of the row:

      * The list is keyed on the CHARACTER name, and on this build the account
        must match as well (locale/english takes the branch in gm_new_get_level
        that checks the account and skips the host check). Both go in the row;
        one without the other silently grants nothing.

      * mServerIP stays 'ALL', and that one is not tidiness. The db core asks
        for rows matching 'ALL' or its own address. At boot its own address is
        right; on a reload it is garbage, because the game core sends the
        reload packet with no body (DBPacket(HEADER_GD_RELOAD_ADMIN, 0, NULL,
        0)) and the db core reads szIP out of it anyway. The live server has
        been seen asking for mServerIP='icate58' -- a fragment of a character
        name left in the buffer. Rows pinned to an address would therefore be
        found at boot and lost, or not, at every reload after it. 'ALL' matches
        either way.
    """
    try:
        pid = int(request.form.get("pid", ""))
    except (TypeError, ValueError):
        flash(t("not_found"), "error")
        return redirect(url_for("dash"))

    rank = (request.form.get("rank", "") or "").strip()
    if rank and rank not in GM_RANK_SET:
        # Not reachable from the form; reachable from a hand-made POST, and an
        # unknown value would be stored happily by MySQL only to be dropped by
        # the server at boot.
        flash(t("act_novalue"), "error")
        return redirect(url_for("player", pid=pid))

    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT p.name AS name, a.login AS login "
                        "FROM player.player p "
                        "LEFT JOIN account.account a ON a.id = p.account_id "
                        "WHERE p.id=%s", (pid,))
            row = cur.fetchone()
            if not row:
                flash(t("not_found"), "error")
                return redirect(url_for("dash"))
            name  = row["name"]
            login = row["login"] or ""

            # Replace rather than update: mName has no unique key, and a table
            # that has collected two rows for one character would otherwise
            # keep the older one alive underneath the new rank.
            cur.execute("DELETE FROM common.gmlist WHERE mName=%s", (name,))
            if rank:
                cur.execute(
                    "INSERT INTO common.gmlist "
                    "(mAccount, mName, mContactIP, mServerIP, mAuthority) "
                    "VALUES (%s, %s, '', 'ALL', %s)", (login, name, rank))
    except Exception:
        flash(t("db_down"), "error")
        return redirect(url_for("player", pid=pid))

    told = gm_ask_for_reload()
    if not told:
        flash(t("gm_noreload"), "error")
    elif rank:
        # The reload makes the game re-read the list and, for every character
        # named in it who happens to be online, apply the rank there and then.
        flash(t("gm_granted").format(name=name, rank=gm_rank_label(rank)))
    else:
        # Removal is the asymmetric case. The reload hands the server the new
        # list, but the server only re-applies ranks for characters IN that
        # list -- one that has just been taken out is not visited, so a player
        # who is online keeps the commands until they log out. Saying so is the
        # difference between a known limit and a bug report.
        flash(t("gm_removed").format(name=name))
    return redirect(url_for("player", pid=pid))

if __name__ == "__main__":
    app.run(host=CONF.get("bind", "0.0.0.0"), port=CONF.get("port", 7788))
