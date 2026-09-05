#!/usr/bin/env python3
# =============================================================
# Metin2 Admin Panel v2 - built for non-technical users
# Config: /usr/local/etc/m2panel.conf  (M2PANEL_CONF to move it)
# =============================================================
import base64, datetime, json, os, random, re, socket, sys, threading, time, hashlib, hmac, secrets, sqlite3, subprocess
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
        6: "Handlarz",
    },
    "en": {
        0: "Steady adventurer", 1: "Metin breaker", 2: "Team companion",
        3: "Gear specialist", 4: "Careful collector", 5: "Wanderer",
        6: "Merchant",
    },
}
BOT_AMBITION_LABELS = {
    "pl": {
        0: "Poziom", 1: "Ekwipunek", 2: "Metiny", 3: "Koń",
        4: "Biolog", 5: "Umiejętności", 6: "Handel",
    },
    "en": {
        0: "Level", 1: "Equipment", 2: "Metins", 3: "Horse",
        4: "Biologist", 5: "Skills", 6: "Trade",
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
# The action id a keeper reports while its stall stands. Must match
# BOT_ACTION_STALL in playerbot_types.h - the id travels in the status file, so
# the two enumerations are one interface.
BOT_ACTION_STALL_ID = 13

BOT_ACTION_LABELS = {
    "pl": {
        0: "Planuje następny ruch", 1: "Podróżuje", 2: "Walczy", 3: "Podnosi łup",
        4: "Regeneruje się", 5: "Wybiera profesję", 6: "Handluje", 7: "Ulepsza EQ",
        8: "Czyta KU", 9: "Wkłada KD", 10: "Organizuje PT", 11: "Robi Biologa",
        12: "Odwiedza Stajennego", 13: "Prowadzi stragan",
    },
    "en": {
        0: "Planning next move", 1: "Travelling", 2: "Fighting", 3: "Picking up loot",
        4: "Recovering", 5: "Choosing profession", 6: "Trading", 7: "Refining gear",
        8: "Reading a skill book", 9: "Socketing a spirit stone", 10: "Organising a party",
        11: "Doing Biologist mission", 12: "Visiting the Stable Boy",
        13: "Keeping a stall",
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


def skill_rank_score(master_type, level):
    """Rank as a number, ordered the way skill_rank_label reads: P beats G10,
    G1 beats M10, M1 beats an unmastered 19. Kept next to that function so the
    two cannot drift apart."""
    master_type, level = int(master_type or 0), int(level or 0)
    if master_type >= 3 or level >= 40:
        return 4000
    if master_type == 2 or level >= 30:
        return 3000 + max(1, level - 29)
    if master_type == 1 or level >= 20:
        return 2000 + max(1, level - 19)
    return level


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
# ---- how the bots behave ----------------------------------------------------
# The same spool once more, but this one needs no helper on the other side: the
# game core reads this file itself, every five seconds, and applies whatever it
# says on the next planning tick. Nothing restarts and nobody is disconnected,
# which is the entire point -- "fewer anglers" used to cost a rebuild.
#
# 100 is neutral for every weight and is what the core assumes for anything the
# file does not mention, so deleting the file puts the world back exactly as the
# build shipped it.
AI_SPOOL     = _env_path("M2PANEL_AI_SPOOL", "/opt/m2spool")
AI_WEIGHTS   = os.path.join(AI_SPOOL, "playerbot_weights.tsv")
AI_W_MIN, AI_W_MAX, AI_W_NEUTRAL = 25, 250, 100

# Name, emoji, and the order they are shown in -- which is the order the core
# tests them in, so the page reads top to bottom like the bot decides.
AI_WEIGHT_KEYS = [
    ("RESTOCK", "🧪"),
    ("REFINE",  "🔨"),
    ("SKILL",   "📖"),
    ("HORSE",   "🐎"),
    ("BIOLOG",  "🧬"),
    ("METIN",   "🗿"),
    ("PARTY",   "👥"),
    ("HUNTING", "🏹"),
    ("LEVEL",   "⚔️"),
    ("FISHING", "🎣"),
    ("TRADE",   "🏪"),
]


def read_ai_weights():
    """What the file says now. Anything missing or unreadable is neutral."""
    vals = {k: AI_W_NEUTRAL for k, _ in AI_WEIGHT_KEYS}
    try:
        with open(AI_WEIGHTS, encoding="utf-8", errors="replace") as fh:
            for line in fh:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = line.replace("\t", " ").split()
                if len(parts) < 2:
                    continue
                name = parts[0].upper()
                if name not in vals:
                    continue
                try:
                    vals[name] = max(AI_W_MIN, min(AI_W_MAX, int(parts[1])))
                except ValueError:
                    pass
    except OSError:
        pass
    return vals


def write_ai_weights(vals):
    """Replace the file in one step.

    Written beside the target and renamed over it, because the game core reads
    it on its own five-second timer and must never catch it half-written -- a
    truncated line would silently reset a weight to neutral.
    """
    body = ["# Metin2 playerbots -- goal weights.",
            "# 25 = rarely, 100 = as the game was built, 250 = often.",
            "# The game core re-reads this within five seconds. Nothing restarts.",
            ""]
    for name, _ in AI_WEIGHT_KEYS:
        body.append("%s\t%d" % (name, vals[name]))
    tmp = AI_WEIGHTS + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(body) + "\n")
    os.replace(tmp, AI_WEIGHTS)


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


# --- Module 5: the goal weights. Four languages like the rest of the panel;
#     the labels are one word each on purpose, because eleven sliders with a
#     paragraph apiece is a wall nobody reads.
T.update({
 "ai_nav":       {"en":"🧠 Bot behaviour","pl":"🧠 Zachowanie botów",
                  "de":"🧠 Bot-Verhalten","tr":"🧠 Bot davranışı"},
 "ai_open":      {"en":"🧠 Open bot behaviour","pl":"🧠 Otwórz zachowanie botów",
                  "de":"🧠 Bot-Verhalten öffnen","tr":"🧠 Bot davranışını aç"},
 "ai_dash_hint": {"en":"Decide what the bots spend their time on \u2014 more metin hunters, fewer anglers, a busier market. Takes effect within five seconds, nothing restarts.",
                  "pl":"Zdecyduj, na co boty poświęcają czas \u2014 więcej łowców metinów, mniej wędkarzy, ruchliwszy targ. Działa w ciągu pięciu sekund, nic się nie restartuje.",
                  "de":"Entscheide, womit die Bots ihre Zeit verbringen \u2014 mehr Metin-Jäger, weniger Angler, ein belebterer Markt. Wirkt binnen fünf Sekunden, nichts startet neu.",
                  "tr":"Botların vaktini neye harcayacağını sen seç \u2014 daha çok metin avcısı, daha az balıkçı, daha hareketli pazar. Beş saniyede etkili olur, hiçbir şey yeniden başlamaz."},
 "ai_intro":     {"en":"Every number here is a preference, not an order. 100 is exactly how the server was built; 25 means a quarter as many bots choose it, 250 two and a half times as many. Surviving a losing fight, choosing a profession and finding a weapon are never affected \u2014 those are not preferences.",
                  "pl":"Każda liczba to preferencja, nie rozkaz. 100 to dokładnie tak, jak serwer został zbudowany; 25 znaczy, że wybierze to cztery razy mniej botów, a 250 \u2014 dwa i pół raza więcej. Ucieczka z przegranej walki, wybór profesji i zdobycie broni nigdy nie podlegają tym suwakom \u2014 to nie są preferencje.",
                  "de":"Jede Zahl hier ist eine Vorliebe, kein Befehl. 100 ist genau so, wie der Server gebaut wurde; 25 heißt, ein Viertel so viele Bots wählen es, 250 zweieinhalbmal so viele. Überleben, Berufswahl und Waffensuche bleiben unberührt \u2014 das sind keine Vorlieben.",
                  "tr":"Buradaki her sayı bir tercih, emir değil. 100, sunucunun yapıldığı hâldir; 25 dörtte bir kadar bot bunu seçer, 250 iki buçuk katı. Hayatta kalma, meslek seçimi ve silah bulma bunlardan etkilenmez \u2014 onlar tercih değildir."},
 "ai_live":      {"en":"Saved. The bots pick this up within five seconds \u2014 no restart, nobody is disconnected.",
                  "pl":"Zapisano. Boty odczytają to w ciągu pięciu sekund \u2014 bez restartu, nikt nie zostaje rozłączony.",
                  "de":"Gespeichert. Die Bots übernehmen das binnen fünf Sekunden \u2014 kein Neustart, niemand fliegt raus.",
                  "tr":"Kaydedildi. Botlar bunu beş saniye içinde alır \u2014 yeniden başlatma yok, kimse düşmez."},
 "ai_failed":    {"en":"Could not write the file \u2014 the shared spool directory is not mounted in this container.",
                  "pl":"Nie udało się zapisać pliku \u2014 współdzielony katalog spool nie jest podmontowany w tym kontenerze.",
                  "de":"Datei konnte nicht geschrieben werden \u2014 das gemeinsame Spool-Verzeichnis ist in diesem Container nicht eingebunden.",
                  "tr":"Dosya yazılamadı \u2014 paylaşılan spool dizini bu kapsayıcıda bağlı değil."},
 "ai_save":      {"en":"Save","pl":"Zapisz","de":"Speichern","tr":"Kaydet"},
 "ai_reset":     {"en":"Everything back to 100","pl":"Wszystko z powrotem na 100","de":"Alles zurück auf 100","tr":"Hepsini 100'e döndür"},
 "ai_rare":      {"en":"rarely","pl":"rzadko","de":"selten","tr":"nadiren"},
 "ai_often":     {"en":"often","pl":"często","de":"oft","tr":"sık"},
 "ai_neutral":   {"en":"as built","pl":"jak w grze","de":"wie gebaut","tr":"yapıldığı gibi"},

 "aiw_RESTOCK":  {"en":"Buying potions","pl":"Kupowanie mikstur","de":"Tränke kaufen","tr":"İksir alma"},
 "aih_RESTOCK":  {"en":"Going back to town the moment the red potions run low.",
                  "pl":"Powrót do miasta, gdy tylko kończą się czerwone mikstury.",
                  "de":"Zurück in die Stadt, sobald die roten Tränke knapp werden.",
                  "tr":"Kırmızı iksirler azalır azalmaz kasabaya dönmek."},
 "aiw_REFINE":   {"en":"The blacksmith","pl":"Kowal","de":"Der Schmied","tr":"Demirci"},
 "aih_REFINE":   {"en":"Upgrading weapons and armour instead of hunting.",
                  "pl":"Ulepszanie broni i pancerza zamiast polowania.",
                  "de":"Waffen und Rüstung aufwerten statt zu jagen.",
                  "tr":"Avlanmak yerine silah ve zırh yükseltmek."},
 "aiw_SKILL":    {"en":"Skill books","pl":"Księgi umiejętności","de":"Skillbücher","tr":"Yetenek kitapları"},
 "aih_SKILL":    {"en":"Reading books to push a skill from master towards grand master.",
                  "pl":"Czytanie ksiąg, by pchnąć umiejętność z M w stronę G.",
                  "de":"Bücher lesen, um einen Skill von M Richtung G zu bringen.",
                  "tr":"Bir yeteneği M'den G'ye taşımak için kitap okumak."},
 "aiw_HORSE":    {"en":"The horse","pl":"Koń","de":"Das Pferd","tr":"At"},
 "aih_HORSE":    {"en":"The stable, and the medal hunt in the Monkey Dungeon.",
                  "pl":"Stajnia i polowanie na medale w Lochu Małp.",
                  "de":"Der Stall und die Medaillenjagd im Affenverlies.",
                  "tr":"Ahır ve Maymun Zindanı'ndaki madalya avı."},
 "aiw_BIOLOG":   {"en":"The Biologist","pl":"Biolog","de":"Der Biologe","tr":"Biyolog"},
 "aih_BIOLOG":   {"en":"Collecting for the Biologist rather than levelling.",
                  "pl":"Zbieranie dla Biologa zamiast bicia poziomów.",
                  "de":"Für den Biologen sammeln statt zu leveln.",
                  "tr":"Seviye yerine Biyolog için toplamak."},
 "aiw_METIN":    {"en":"Metin stones","pl":"Kamienie Metin","de":"Metinsteine","tr":"Metin taşları"},
 "aih_METIN":    {"en":"Hunting metins instead of ordinary monsters.",
                  "pl":"Polowanie na metiny zamiast na zwykłe potwory.",
                  "de":"Metins jagen statt gewöhnlicher Monster.",
                  "tr":"Sıradan canavar yerine metin avlamak."},
 "aiw_PARTY":    {"en":"Parties","pl":"Grupy (PT)","de":"Gruppen","tr":"Gruplar"},
 "aih_PARTY":    {"en":"Fighting together rather than each bot for itself.",
                  "pl":"Walka razem, a nie każdy bot na własną rękę.",
                  "de":"Gemeinsam kämpfen statt jeder für sich.",
                  "tr":"Herkes kendi başına değil, birlikte savaşmak."},
 "aiw_HUNTING":  {"en":"Hunting missions","pl":"Misje polowania","de":"Jagdmissionen","tr":"Av görevleri"},
 "aih_HUNTING":  {"en":"The level-up hunt on the map the mission points at.",
                  "pl":"Polowanie na awans na mapie, którą wskazuje misja.",
                  "de":"Die Aufstiegsjagd auf der Karte, die die Mission nennt.",
                  "tr":"Görevin gösterdiği haritada seviye avı."},
 "aiw_LEVEL":    {"en":"Plain grinding","pl":"Zwykłe bicie potworów","de":"Schlichtes Grinden","tr":"Düz grind"},
 "aih_LEVEL":    {"en":"What a bot does when nothing else is asking for it. Raise this and the errands lose.",
                  "pl":"To, co bot robi, gdy nic innego się nie dopomina. Podnieś, a sprawunki przegrają.",
                  "de":"Was ein Bot tut, wenn nichts anderes ruft. Höher, und die Besorgungen verlieren.",
                  "tr":"Başka bir şey çağırmadığında botun yaptığı şey. Yükselt, işler geri kalır."},
 "aiw_FISHING":  {"en":"Fishing","pl":"Wędkowanie","de":"Angeln","tr":"Balık tutma"},
 "aih_FISHING":  {"en":"How many bots take up fishing at all. Decided once per bot, so a change reaches the next generation of anglers.",
                  "pl":"Ilu botów w ogóle łowi. Rozstrzygane raz na bota, więc zmiana obejmuje kolejne pokolenie wędkarzy.",
                  "de":"Wie viele Bots überhaupt angeln. Einmal pro Bot entschieden, eine Änderung trifft also die nächsten Angler.",
                  "tr":"Kaç botun balık tuttuğu. Bot başına bir kez belirlenir, değişiklik sonraki balıkçılara işler."},
 "aiw_TRADE":    {"en":"Market stalls","pl":"Stragany","de":"Marktstände","tr":"Pazar tezgahları"},
 "aih_TRADE":    {"en":"How many bots keep a private shop open. Merchants always do, whatever this says.",
                  "pl":"Ilu botów trzyma otwarty stragan. Handlarze robią to zawsze, niezależnie od tego suwaka.",
                  "de":"Wie viele Bots einen Laden offen halten. Händler tun es immer, egal was hier steht.",
                  "tr":"Kaç botun tezgahı açık tuttuğu. Tüccarlar bundan bağımsız olarak hep açar."},
})


# --- Module 6: the weekly season and the all-time records.
T.update({
 "se_nav":       {"en":"🏆 Weekly season","pl":"🏆 Sezon tygodniowy",
                  "de":"🏆 Wochensaison","tr":"🏆 Haftalık sezon"},
 "se_open":      {"en":"🏆 Open the season","pl":"🏆 Otwórz sezon",
                  "de":"🏆 Saison öffnen","tr":"🏆 Sezonu aç"},
 "se_dash_hint": {"en":"Who did the most this week, and every record the world has ever set.",
                  "pl":"Kto zrobił najwięcej w tym tygodniu i wszystkie rekordy, jakie świat ustanowił.",
                  "de":"Wer diese Woche am meisten geschafft hat, und jeder Rekord, den die Welt je aufgestellt hat.",
                  "tr":"Bu hafta en çok kim başardı ve dünyanın şimdiye kadarki tüm rekorları."},
 "se_intro":     {"en":"Seven days of metins, bosses and refines that landed on +7 or better. Level, horse and gold are shown for context but are deliberately not scored: they are what a bot IS, not what it did this week.",
                  "pl":"Siedem dni metinów, bossów i ulepszeń, które weszły na +7 lub wyżej. Poziom, koń i yang są pokazane dla kontekstu, ale celowo nie liczą się do wyniku: to jest to, KIM bot jest, a nie co zrobił w tym tygodniu.",
                  "de":"Sieben Tage Metins, Bosse und Aufwertungen, die auf +7 oder besser gelandet sind. Level, Pferd und Yang stehen als Kontext dabei, zählen aber bewusst nicht: sie sagen, was ein Bot IST, nicht was er diese Woche getan hat.",
                  "tr":"Yedi günlük metin, boss ve +7 ve üzerine oturan yükseltmeler. Seviye, at ve yang bağlam için gösterilir ama bilerek puana girmez: onlar botun NE olduğunu söyler, bu hafta ne yaptığını değil."},
 "se_back_map":  {"en":"Live map","pl":"Mapa na żywo","de":"Live-Karte","tr":"Canlı harita"},
 "se_records":   {"en":"Server records","pl":"Rekordy serwera","de":"Serverrekorde","tr":"Sunucu rekorları"},
 "se_records_h": {"en":"All time, since the world was created.","pl":"Od początku istnienia świata.",
                  "de":"Seit Bestehen der Welt.","tr":"Dünyanın kurulduğu günden beri."},
 "se_empty":     {"en":"Nobody has done anything countable in the last seven days.",
                  "pl":"Nikt nie zrobił niczego policzalnego w ciągu ostatnich siedmiu dni.",
                  "de":"In den letzten sieben Tagen hat niemand etwas Zählbares getan.",
                  "tr":"Son yedi günde kimse sayılabilir bir şey yapmadı."},
 "se_score":     {"en":"Score","pl":"Wynik","de":"Punkte","tr":"Puan"},
 "se_metins":    {"en":"Metins","pl":"Metiny","de":"Metins","tr":"Metinler"},
 "se_bosses":    {"en":"Bosses","pl":"Bossy","de":"Bosse","tr":"Bosslar"},
 "se_refines":   {"en":"+7 and up","pl":"+7 i wyżej","de":"+7 und mehr","tr":"+7 ve üzeri"},
 "se_deaths":    {"en":"Deaths","pl":"Zgony","de":"Tode","tr":"Ölümler"},
 "se_level":     {"en":"Level","pl":"Poziom","de":"Level","tr":"Seviye"},
 "se_horse":     {"en":"Horse","pl":"Koń","de":"Pferd","tr":"At"},
 "se_bot":       {"en":"Bot","pl":"Bot","de":"Bot","tr":"Bot"},
 "se_r_level":   {"en":"Highest level","pl":"Najwyższy poziom","de":"Höchstes Level","tr":"En yüksek seviye"},
 "se_r_metins":  {"en":"Most metins","pl":"Najwięcej metinów","de":"Meiste Metins","tr":"En çok metin"},
 "se_r_bosses":  {"en":"Most bosses","pl":"Najwięcej bossów","de":"Meiste Bosse","tr":"En çok boss"},
 "se_r_refines": {"en":"Most +7 and up","pl":"Najwięcej +7 i wyżej","de":"Meiste +7 und mehr","tr":"En çok +7 ve üzeri"},
 "se_r_horse":   {"en":"Best horse","pl":"Najlepszy koń","de":"Bestes Pferd","tr":"En iyi at"},
 "se_r_gold":    {"en":"Most yang","pl":"Najwięcej yang","de":"Meiste Yang","tr":"En çok yang"},
})

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
  ("🏯", {"en":"Shinsoo City","pl":"Miasto Shinsoo","de":"Shinsoo-Stadt","tr":"Shinsoo Şehri"}, "474300 954800"),
  ("🏮", {"en":"Chunjo City","pl":"Miasto Chunjo","de":"Chunjo-Stadt","tr":"Chunjo Şehri"}, "65900 155600"),
  ("⛩️", {"en":"Jinno City","pl":"Miasto Jinno","de":"Jinno-Stadt","tr":"Jinno Şehri"}, "963500 279700"),
  ("🏘️", {"en":"Bokjung (M2)","pl":"Bokjung (M2)","de":"Bokjung (M2)","tr":"Bokjung (M2)"}, "145500 240000"),
  ("⚔️", {"en":"Orc Valley","pl":"Dolina Orków","de":"Orktal","tr":"Ork Vadisi"}, "270400 739900"),
  ("🏜️", {"en":"Yongbi Desert","pl":"Pustynia Yongbi","de":"Yongbi-Wüste","tr":"Yongbi Çölü"}, "221900 502700"),
  ("❄️", {"en":"Mount Sohan","pl":"Góra Sohan","de":"Sohan-Berg","tr":"Sohan Dağı"}, "375200 174900"),
  ("🔥", {"en":"Fireland","pl":"Ognista Ziemia","de":"Feuerland","tr":"Ateş Ülkesi"}, "597800 622200"),
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
<h3 class="help">{{t('se_nav')}}</h3>
<p class="muted">{{t('se_dash_hint')}}</p>
<a class="btn" href="{{url_for('season')}}">{{t('se_open')}}</a>
</div>
<div class="card">
<h3 class="help">{{t('ai_nav')}}</h3>
<p class="muted">{{t('ai_dash_hint')}}</p>
<a class="btn" href="{{url_for('ai_weights')}}">{{t('ai_open')}}</a>
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

# The season table and the record tiles. Public, like the live map: this is the
# page an operator links to, not an admin tool.
TPL_SEASON = BASE.replace("__BODY__", """
<p><a href="{{url_for('live_map')}}">&larr; {{t('se_back_map')}}</a></p>
<div class="card">
<h3>{{t('se_nav')}}</h3>
<p class="muted">{{t('se_intro')}}</p>
{% if s.error %}<p class="muted">{{s.error}}</p>{% endif %}
</div>

<div class="card">
<h3>{{t('se_records')}}</h3>
<p class="muted">{{t('se_records_h')}}</p>
<div style="display:flex;flex-wrap:wrap;gap:10px">
{% for key, label in [('level','se_r_level'), ('metins','se_r_metins'),
                      ('bosses','se_r_bosses'), ('refines','se_r_refines'),
                      ('horse','se_r_horse'), ('gold','se_r_gold')] %}
  {% if s.records.get(key) %}
  <div style="flex:1 1 150px;border:1px solid rgba(128,128,128,.35);border-radius:8px;padding:10px">
    <div class="muted" style="font-size:12px">{{t(label)}}</div>
    <div style="font-size:20px;font-weight:700">{{ '{:,}'.format(s.records[key]['level']|int).replace(',', ' ') }}</div>
    <div class="muted">{{s.records[key]['name']}}</div>
  </div>
  {% endif %}
{% endfor %}
</div>
</div>

<div class="card">
<h3>{{t('se_nav')}} &mdash; {{s.days}} dni</h3>
{% if not s.rows %}
<p class="muted">{{t('se_empty')}}</p>
{% else %}
<div style="overflow-x:auto">
<table style="width:100%;border-collapse:collapse">
<tr style="text-align:left">
  <th>#</th><th>{{t('se_bot')}}</th><th>{{t('se_score')}}</th>
  <th>{{t('se_metins')}}</th><th>{{t('se_bosses')}}</th><th>{{t('se_refines')}}</th>
  <th>{{t('se_deaths')}}</th><th>{{t('se_level')}}</th><th>{{t('se_horse')}}</th>
</tr>
{% for r in s.rows %}
<tr style="border-top:1px solid rgba(128,128,128,.25)">
  <td class="muted">{{loop.index}}</td>
  <td><b>{{r.name}}</b></td>
  <td><b>{{r.score}}</b></td>
  <td>{{r.metins}}</td><td>{{r.bosses}}</td><td>{{r.refines}}</td>
  <td class="muted">{{r.deaths}}</td>
  <td class="muted">{{r.level}}</td>
  <td class="muted">{{r.horse}}</td>
</tr>
{% endfor %}
</table>
</div>
{% endif %}
</div>""")



# The bot behaviour sliders. One range input per weight, each with the sentence
# that says what it actually changes -- an unlabelled slider called "BIOLOG" is
# a number, not a control. No %-formatting anywhere in here: BASE is full of
# CSS percentages and would eat it.
TPL_AI = BASE.replace("__BODY__", """
<p><a href="{{url_for('dash')}}">{{t('back_players')}}</a></p>
<div class="card">
<h3>{{t('ai_nav')}}</h3>
<p class="muted">{{t('ai_intro')}}</p>
</div>

<div class="card">
<form method="post">
<input type="hidden" name="_csrf" value="{{csrf_token}}">
{% for name, emoji in keys %}
<div style="margin-bottom:18px">
  <h3 style="margin:0 0 2px">{{emoji}} {{t('aiw_' ~ name)}}
      <span class="badge" id="v_{{name}}">{{cur[name]}}</span></h3>
  <p class="muted" style="margin:0 0 6px">{{t('aih_' ~ name)}}</p>
  <input type="range" name="{{name}}" id="s_{{name}}" min="{{wmin}}" max="{{wmax}}"
         step="5" value="{{cur[name]}}" style="width:100%"
         oninput="document.getElementById('v_{{name}}').textContent=this.value">
  <div class="muted" style="display:flex;justify-content:space-between;font-size:12px">
    <span>{{wmin}} — {{t('ai_rare')}}</span>
    <span>{{wneutral}} — {{t('ai_neutral')}}</span>
    <span>{{wmax}} — {{t('ai_often')}}</span>
  </div>
</div>
{% endfor %}
<button type="button" onclick="m2aiReset()">{{t('ai_reset')}}</button>
<button class="big" style="margin-top:10px">{{t('ai_save')}}</button>
</form></div>
<script>
function m2aiReset(){
  document.querySelectorAll('input[type=range]').forEach(function(s){
    s.value = {{wneutral}};
    document.getElementById('v_' + s.name).textContent = s.value;
  });
}
</script>""")



MAP_I18N = {
 "pl": {
  "title":"Mapa świata na żywo — Chunjo","live":"NA ŻYWO (1,5 s)","subtitle":"Interaktywny podgląd pozycji i rozwoju botów w czasie rzeczywistym",
  "player_panel":"Panel graczy","play_browser":"Graj w przeglądarce","show_bots":"Pokaż boty","names_levels":"Nicki i poziomy","pt_only":"Tylko w grupie (PT)",
  "level":"Poziom","all":"Wszystkie","map":"Mapa","m1":"M1 — Joan","m2":"M2 — Bokjung","m3":"M3 — Waryong","monkey":"Łatwy Loch Małp","orc":"Dolina Orków","desert":"Pustynia Yongbi","heat":"Mapa cieplna","heat_deaths":"Zgony botów","heat_metins":"Rozbite metiny","heat_skills":"Awanse umiejętności","search":"🔍 Szukaj bota (np. botarek)...",
  "solo_bot":"Bot solo","party_bot":"W grupie (PT)","metin_fight":"Walka z Metinem","loading":"Ładowanie...","world_stats":"Statystyki świata","active_bots":"Aktywne boty",
  "in_parties":"W grupach (PT)","avg_level":"Średni poziom","max_level":"Maks. poziom","rankings":"Rankingi botów","rank_level":"Poziom","rank_weapon":"Broń","rank_armor":"Zbroja",
  "rank_weapon30":"Bronie 30 Lv","rank_items":"Przedmioty","rank_horse":"Koń","rank_biologist":"Biolog","rank_hunting":"Polowanie","rank_shops":"Otwarte sklepy","rank_skills":"Umiejętności","rank_plus9":"Przedmiot +9","rank_stall_open":"Stragan otwarty","rank_empty":"Brak danych rankingu.","rank_show":"Pokaż","rank_search":"Szukaj w rankingu...","none":"Brak","items_short":"przedm.",
  "horse_lv":"Koń Lv","visible":"Widocznych","characters":"postaci","in_group":"W grupie [PT]","solo":"Solo","player":"GRACZ","bot":"Bot","class":"Klasa","action":"Akcja","status":"Status","personality":"Osobowość","ambition":"Ambicja","current_goal":"Aktualny cel",
  "coordinates":"Koordynaty","open_inventory":"Kliknij, aby otworzyć ekwipunek i EQ","loading_character":"Ładowanie ekwipunku i statystyk postaci","error":"Błąd","not_found":"Nie znaleziono danych",
  "teleport_me":"Teleportuj moją postać w grze (1 klik)","position":"Pozycja","horse":"Koń","biologist":"Biolog","bio_stage":"Etap Biologa","hunting":"Polowanie","no_data":"Brak danych",
  "stats":"Statystyki","unspent_stats":"Nierozdane: {n} pkt statystyk","skills":"Umiejętności","profession_none":"Nie wybrano","profession_pending":"Profesja nie została jeszcze wybrana.","depot":"Magazyn","depot_empty":"Magazyn jest pusty.",
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
  "level":"Level","all":"All","map":"Map","m1":"M1 — Joan","m2":"M2 — Bokjung","m3":"M3 — Waryong","monkey":"Easy Monkey Dungeon","orc":"Orc Valley","desert":"Yongbi Desert","heat":"Heatmap","heat_deaths":"Bot deaths","heat_metins":"Metins broken","heat_skills":"Skill-ups","search":"🔍 Find a bot (e.g. botarek)...",
  "solo_bot":"Solo bot","party_bot":"In party (PT)","metin_fight":"Fighting a Metin","loading":"Loading...","world_stats":"World statistics","active_bots":"Active bots",
  "in_parties":"In parties (PT)","avg_level":"Average level","max_level":"Max level","rankings":"Bot rankings","rank_level":"Level","rank_weapon":"Weapon","rank_armor":"Armour",
  "rank_weapon30":"Lv 30 Weapons","rank_items":"Items","rank_horse":"Horse","rank_biologist":"Biologist","rank_hunting":"Hunting","rank_shops":"Open shops","rank_skills":"Skills","rank_plus9":"Item +9","rank_stall_open":"Stall open","rank_empty":"No ranking data.","rank_show":"Show","rank_search":"Search ranking...","none":"None","items_short":"items",
  "horse_lv":"Horse Lv","visible":"Visible","characters":"characters","in_group":"In party [PT]","solo":"Solo","player":"PLAYER","bot":"Bot","class":"Class","action":"Action","status":"Status","personality":"Personality","ambition":"Ambition","current_goal":"Current goal",
  "coordinates":"Coordinates","open_inventory":"Click to open inventory and equipment","loading_character":"Loading character equipment and statistics","error":"Error","not_found":"No data found",
  "teleport_me":"Teleport my in-game character (one click)","position":"Position","horse":"Horse","biologist":"Biologist","bio_stage":"Biologist stage","hunting":"Hunting","no_data":"No data",
  "stats":"Statistics","unspent_stats":"Unspent: {n} stat points","skills":"Skills","profession_none":"Not selected","profession_pending":"The profession has not been selected yet.","depot":"Depot","depot_empty":"The depot is empty.",
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
          <button type="button" class="btn lvl-btn" style="padding:4px 8px;font-size:12px" onclick="setLevelFilter('1-15', this)">1-15</button>
          <button type="button" class="btn lvl-btn" style="padding:4px 8px;font-size:12px" onclick="setLevelFilter('16-25', this)">16-25</button>
          <button type="button" class="btn lvl-btn" style="padding:4px 8px;font-size:12px" onclick="setLevelFilter('26-35', this)">26-35</button>
          <button type="button" class="btn lvl-btn" style="padding:4px 8px;font-size:12px" onclick="setLevelFilter('36+', this)">36+</button>
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
          <option value="skills">{{m.heat_skills}}</option>
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
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('shops', this)" style="font-size:11px;padding:3px 6px">🏪 {{m.rank_shops}}</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('skills', this)" style="font-size:11px;padding:3px 6px">✨ {{m.rank_skills}}</button>
          <button type="button" class="btn btn-sm rank-tab" onclick="setRankCategory('plus9', this)" style="font-size:11px;padding:3px 6px">🔥 {{m.rank_plus9}}</button>
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
/* The depot window floats free of #botModal so that dragging it and closing
   the bot modal stay independent of each other, and so it survives a modal
   close/reopen. The grid inside it is the equipment modal's own
   .m2-grid-frame/.m2-grid-bg/.m2-item-overlay, reused unchanged. */
.m2-safebox-window {
  position: fixed;
  top: 90px;
  right: 24px;
  /* Above .modal-overlay (z-index 9999): both can be open at once, and this
     must float over the modal's backdrop rather than be buried by it merely
     for coming earlier in the document. */
  z-index: 10001;
  width: 186px;
  background: #1a150e;
  border: 1px solid #332814;
  border-radius: 6px;
  box-shadow: 0 8px 24px rgba(0,0,0,0.65);
  padding: 8px;
  display: none;
}
.m2-safebox-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 6px;
  padding-bottom: 4px;
  border-bottom: 1px solid #3d3119;
  cursor: move;
  user-select: none;
  color: var(--gold);
  font-size: 12px;
  font-weight: 700;
}
.m2-safebox-close {
  background: none;
  border: none;
  color: #a89070;
  cursor: pointer;
  font-size: 15px;
  line-height: 1;
  padding: 0 2px;
}
.m2-safebox-close:hover { color: #ffd700; }
.m2-safebox-empty {
  color: #6b6252;
  font-size: 11px;
  text-align: center;
  padding: 10px 0;
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
<!-- Depot (safebox), floating and draggable; see .m2-safebox-window. -->
<div id="m2SafeboxWindow" class="m2-safebox-window">
  <div class="m2-safebox-header" id="m2SafeboxHeader">
    <span>📦 <span id="m2SafeboxTitle">{{m.depot}}</span></span>
    <button type="button" class="m2-safebox-close" onclick="closeSafeboxWindow()">&times;</button>
  </div>
  <div class="m2-grid-frame">
    <div class="m2-grid-bg" id="m2SafeboxGridBg"></div>
    <div id="m2SafeboxItemOverlay" class="m2-item-overlay"></div>
  </div>
  <div id="m2SafeboxEmpty" class="m2-safebox-empty" style="display:none">{{m.depot_empty}}</div>
</div>

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

window.addEventListener('DOMContentLoaded', function(){ applyMapBackdrop(); });

function applyMapBackdrop() {
  var viewport = document.getElementById('mapViewport');
  if (!viewport) return;
  // Stretched to the whole viewport on purpose: markers are positioned as a
  // percentage of the same bounds the tile was rendered from, so the two line
  // up exactly whatever the panel is resized to.
  viewport.style.backgroundImage = "url('/api/map_tile/" + g_selectedMap + "')";
  viewport.style.backgroundSize = '100% 100%';
  viewport.style.backgroundRepeat = 'no-repeat';
  viewport.style.backgroundPosition = 'center';
}

function setMapFilter(mapIndex) {
  g_selectedMap = parseInt(mapIndex, 10) || 21;
  applyMapBackdrop();
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
  var kindNow = kindEl ? kindEl.value : 'deaths';
  var rgb = '255,72,58';
  if (kindNow === 'metins') rgb = '255,105,205';
  else if (kindNow === 'skills') rgb = '96,214,124';
  var html = '';
  for (var i = 0; i < g_heatCells.length; i++) {
    var c = g_heatCells[i];
    // Square root, so the visible area tracks the count instead of the radius --
    // otherwise one busy cell swallows the whole map.
    var w = Math.sqrt(c.n / g_heatMax);
    // The floor is what makes the quiet ground readable. A cell with one death
    // in it still gets a small dot, so the eye sees the whole shape of where
    // bots have been and the blobs stand out of that rather than floating on an
    // empty map.
    var size = (11 + w * 62).toFixed(0);
    var alpha = (0.20 + w * 0.55).toFixed(2);
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
    } else if (g_selectedRankCategory === 'shops') {
      detailStr = '<span style="color:#22d3ee;font-weight:700">🏪 ' + I18N.rank_stall_open + '</span>' +
                  (b.stall_map ? ' <span class="muted" style="font-size:11px">' + b.stall_map + '</span>' : '');
    } else if (g_selectedRankCategory === 'skills') {
      // The grade colours match the bot card: P pink, G purple, M blue.
      var gr = b.skill_rank || '0';
      var grColor = gr.charAt(0) === 'P' ? '#f472b6' :
                    (gr.charAt(0) === 'G' ? '#c084fc' :
                    (gr.charAt(0) === 'M' ? '#38bdf8' : '#a1a1aa'));
      detailStr = '<span style="color:' + grColor + ';font-weight:700">' + gr + '</span>' +
                  ' <span style="color:#e5e7eb">' + (b.skill_rank_name || I18N.none) + '</span>';
    } else if (g_selectedRankCategory === 'plus9') {
      var p9win = b.item_window === 'EQUIPMENT'
          ? '<span style="background:#15803d;color:#fff;font-size:9px;padding:1px 4px;border-radius:3px;margin-left:4px">EQ</span>'
          : '<span style="background:#374151;color:#bbb;font-size:9px;padding:1px 4px;border-radius:3px;margin-left:4px">Plecak</span>';
      var p9icon = b.weapon_vnum ? '<img src="' + getItemIconUrl(b.weapon_vnum) + '" style="width:16px;height:16px;vertical-align:middle;margin-right:4px;image-rendering:pixelated">' : '';
      detailStr = p9icon + '<span style="color:#f97316;font-weight:700">' + (b.weapon_name || '+9') + '</span>' + p9win;
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

    // Bands that mean something now the population reaches level 37: the old
    // 1-5/6-10/11-15/16+ put almost every bot in the last bucket, which is the
    // same as having no filter at all. 36+ is the Orc Valley cohort.
    if (g_selectedLevel === '1-15' && b.level > 15) return;
    if (g_selectedLevel === '16-25' && (b.level < 16 || b.level > 25)) return;
    if (g_selectedLevel === '26-35' && (b.level < 26 || b.level > 35)) return;
    if (g_selectedLevel === '36+' && b.level < 36) return;

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

// Every bonus line the engine can roll, with the wording the game itself uses.
// Generated from three sources that have to agree: the server's EApplyTypes
// enum fixes the numbers, the client's AFFECT_DICT maps each number to a locale
// key, and the client's locale_game.txt holds the text. Nothing here is
// translated by hand, which is what the previous table did - it had 71 and 72
// the wrong way round (71 is skill damage, 72 is average damage), classed the
// two post-kill recovery chances as plain numbers rather than percentages, and
// had no entry at all for 87-91, so an ice/earth/dark resistance showed up as
// "Bonus #89".
//
// The text keeps the client's own placeholder, and that is what says how to
// print the value: %d%% is a percentage, %d a plain number, %.1f a
// multiplier, and no placeholder at all means the client shows the line with no
// number after it. Ids the client has no text for are absent on purpose.
var APPLY_META = {
  1: {pl: "Max PŻ: +%d", en: "Max. HP +%d", f: "flat"},
  2: {pl: "Max PE: +%d", en: "Max. SP +%d", f: "flat"},
  3: {pl: "Witalność +%d", en: "Vitality +%d", f: "flat"},
  4: {pl: "Inteligencja +%d", en: "Intelligence +%d", f: "flat"},
  5: {pl: "Siła: +%d", en: "Strength +%d", f: "flat"},
  6: {pl: "Zręczność +%d", en: "Dexterity +%d", f: "flat"},
  7: {pl: "Szybkość Ataku +%d%%", en: "Attack Speed +%d%%", f: "percent"},
  8: {pl: "Szybkość Ruchu %d%%", en: "Moving Speed %d%%", f: "percent"},
  9: {pl: "Szybkość Zaklęcia +%d%%", en: "Casting Speed +%d%%", f: "percent"},
  10: {pl: "Regeneracja PŻ: +%d%%", en: "HP Regeneration +%d%%", f: "percent"},
  11: {pl: "Regeneracja PE: +%d%%", en: "SP Regeneration +%d%%", f: "percent"},
  12: {pl: "Szansa na Otrucie %d%%", en: "Poisoning chance %d%%", f: "percent"},
  13: {pl: "Szansa na Omdlenie %d%%", en: "Blackout chance %d%%", f: "percent"},
  14: {pl: "Szansa na Spowolnienie %d%%", en: "Slowing chance %d%%", f: "percent"},
  15: {pl: "Szansa na cios krytyczny +%d%%", en: "Chance of critical hit +%d%% ", f: "percent"},
  16: {pl: "Szansa na przeszywające Uderzenie: %d%% ", en: "%d%% Chance for piercing Hits", f: "percent"},
  17: {pl: "Silny przeciwko Ludziom +%d%%", en: "Strong against Half Humans +%d%%", f: "percent"},
  18: {pl: "Silny przeciwko Zwierzętom +%d%%", en: "Strong against Animals +%d%%", f: "percent"},
  19: {pl: "Silny przeciwko Orkom +%d%%", en: "Strong against Orcs +%d%%", f: "percent"},
  20: {pl: "Silny przeciwko Mistykom +%d%%", en: "Strong against Mystics +%d%%", f: "percent"},
  21: {pl: "Silny przeciwko Nieumarłym +%d%%", en: "Strong against Undead +%d%%", f: "percent"},
  22: {pl: "Silny przeciwko Diabłom +%d%%", en: "Strong against Devils +%d%%", f: "percent"},
  23: {pl: "%d%% obrażeń będzie dodanych do PŻ", en: "%d%% damage  will be absorbed by HP", f: "percent"},
  24: {pl: "%d%% obrażeń będzie dodanych do PE", en: "%d%% damage will be absorbed by SP", f: "percent"},
  25: {pl: "%d%% Szansa na kradzież PE", en: "%d%% chance to rob SP", f: "percent"},
  26: {pl: "Szansa na odzyskanie PE: %d%%", en: "%d%% Chance to get back SP when hit", f: "percent"},
  27: {pl: "Szansa na blok ciosów %d%%", en: "Chance to block a close-combat attack %d%% ", f: "percent"},
  28: {pl: "Szansa na uniknięcie Strzały: %d%%", en: "Chance to avoid Arrows %d%%", f: "percent"},
  29: {pl: "Odporność na Miecze: %d%%", en: "Sword Defence %d%%", f: "percent"},
  30: {pl: "Odporność na Broń Dwuręczną: %d%%", en: "Two-Handed Defence %d%%", f: "percent"},
  31: {pl: "Odporność na Sztylety: %d%%", en: "Dagger Defence %d%%", f: "percent"},
  32: {pl: "Odporność na Dzwony: %d%%", en: "Bell Defence %d%%", f: "percent"},
  33: {pl: "Odporność na Wachlarze: %d%%", en: "Fan Defence %d%%", f: "percent"},
  34: {pl: "Odporność na Strzały: %d%%", en: "Arrow Resistance %d%%", f: "percent"},
  35: {pl: "Odporność na Ogień: %d%%", en: "Fire Resistance %d%%", f: "percent"},
  36: {pl: "Odporność na Błyskawice: %d%%", en: "Lightning Resistance %d%%", f: "percent"},
  37: {pl: "Odporność na Magię: %d%%", en: "Magic Resistance %d%%", f: "percent"},
  38: {pl: "Odporność na Wiatr: %d%%", en: "Wind Resistance %d%%", f: "percent"},
  39: {pl: "%d%% Szansa na odbicie Ciosu", en: "%d%% Chance to reflect close combat hits  ", f: "percent"},
  40: {pl: "Szansa na odbicie Klątwy: %d%%", en: "Chance to reflect Curse: %d%%", f: "percent"},
  41: {pl: "Odporność na Trucizny: %d%%", en: "Poison Resistance %d%%", f: "percent"},
  42: {pl: "Szansa na odzyskanie PE: %d%%", en: "%d%% Chance to restore SP", f: "percent"},
  43: {pl: "Szansa na Bonus DOŚ: %d%%", en: "%d%% Chance for EXP Bonus", f: "percent"},
  44: {pl: "Szansa na podwójną ilość Yang: %d%%", en: "%d%% Chance to drop double Yang", f: "percent"},
  45: {pl: "Szansa na podwójną ilość Przedmiotów: %d%%", en: "%d%% Chance to drop double the Items", f: "percent"},
  46: {pl: "Mikstury %d%% efekt podniesiony", en: "Potion %d%% effect raise", f: "percent"},
  47: {pl: "Szansa na odzyskanie PŻ: %d%%", en: "%d%% Chance, to restore HP", f: "percent"},
  48: {pl: "Odporność na Omdlenia", en: "Defence against blackouts", f: "boolean"},
  49: {pl: "Odporność na Spowolnienia", en: "Defence against slowing", f: "boolean"},
  50: {pl: "Niewrażliwy na Upadek", en: "Immune against falling down", f: "boolean"},
  52: {pl: "Zasięg Łuku +%dm", en: "Arc Range +%dm", f: "flat"},
  53: {pl: "Wartość Ataku +%d", en: "Attack Value +%d", f: "flat"},
  54: {pl: "Obrona +%d", en: "Defence +%d", f: "flat"},
  55: {pl: "Wartość Magicznego Ataku: +%d", en: "Magical Attack Value +%d", f: "flat"},
  56: {pl: "Magiczna Obrona: +%d", en: "Magical Defence +%d", f: "flat"},
  58: {pl: "Max Wytrzymałość: +%d", en: "Max. Endurance +%d", f: "flat"},
  59: {pl: "Silny przeciwko Wojownikom +%d%%", en: "Strong against Warriorr +%d%%", f: "percent"},
  60: {pl: "Silny przeciwko Ninja +%d%%", en: "Strong against Ninjas +%d%%", f: "percent"},
  61: {pl: "Silny przeciwko Sura +%d%%", en: "Strong against Sura +%d%%", f: "percent"},
  62: {pl: "Silny przeciwko Szamanom +%d%%", en: "Strong against Shamans +%d%%", f: "percent"},
  63: {pl: "Silny przeciwko Potworom +%d%%", en: "Strength against monsters +%d%%", f: "percent"},
  64: {pl: "Wartość Ataku: +%d%%", en: "Attack Value +%d%%", f: "percent"},
  65: {pl: "Obrona: +%d%%", en: "Defence +%d%%", f: "percent"},
  66: {pl: "Punkty Doświadczenia: +%d%%", en: "EXP +%d%%", f: "percent"},
  67: {pl: "Szansa na zdobycie Przedmiotów pomnożona o %.1f", en: "Chance of capturing Items multiplied with %.1f", f: "multiplier"},
  68: {pl: "Szansa na zdobycie Yang pomnożona o %.1f", en: "Chance of capturing Yang multiplied with %.1f", f: "multiplier"},
  69: {pl: "Maks. PŻ +%d%%", en: "Max. HP +%d%%", f: "percent"},
  70: {pl: "Maks. PE +%d%% ", en: "Max. SP +%d%% ", f: "percent"},
  71: {pl: "Obrażenie Umiejętności: %d%%", en: "Skill Damage %d%%", f: "percent"},
  72: {pl: "Średnie Obrażenia: %d%%", en: "Average Damage %d%%", f: "percent"},
  73: {pl: "Odporność na Obrażenia Umiejętności %d%%", en: "Resistance against Skill Damage %d%%", f: "percent"},
  74: {pl: "Odporność na Obrażenia: %d%%", en: "Average Damage Resistance %d%%", f: "percent"},
  75: {pl: "iCafe DOŚ Bonus +%d%%", en: "iCafe EXP Bonus +%d%%", f: "percent"},
  76: {pl: "iCafe Szansa na zdobycie Przedmiotów plus %.1f%%", en: "iCafe Chance of capturing Items plus %.1f%%", f: "percent_decimal"},
  78: {pl: "Odporność na Wojowników: %d%%", en: "Defence chance against warrior attacks: %d%%", f: "percent"},
  79: {pl: "Odporność na Ninja: %d%%", en: "Defence chance against ninja attacks: %d%%", f: "percent"},
  80: {pl: "Odporność na Sura: %d%%", en: "Defence chance against sura attacks: %d%%", f: "percent"},
  81: {pl: "Odporność na Szamanów: %d%%", en: "Defence chance against shaman attacks: %d%%", f: "percent"},
  82: {pl: "Energia %d", en: "Energy %d ", f: "flat"},
  84: {pl: "Bonus kostiumu %d%% ", en: "Costume bonus %d%% ", f: "percent"},
  85: {pl: "Magiczny atak +%d%%", en: "Magic attack +%d%%", f: "percent"},
  86: {pl: "Magiczny/krótkodystansowy atak +%d%%", en: "Magic/melee attack +%d%%", f: "percent"},
  87: {pl: "Odporność na lód +%d%%", en: "Ice resistance +%d%%", f: "percent"},
  88: {pl: "Odporność na ziemię +%d%%", en: "Earth resistance +%d%%", f: "percent"},
  89: {pl: "Odporność na mrok +%d%%", en: "Resistance against darkness +%d%%", f: "percent"},
  90: {pl: "Odporność na cios krytyczny +%d%%", en: "Resistance against critical hits +%d%%", f: "percent"},
  91: {pl: "Odporność na przeszywający cios +%d%%", en: "Resistance against piercing hits +%d%%", f: "percent"}
};

// One formatter for both the fixed bonuses an item is made with and the random
// lines rolled onto it, so the same id can never be worded or punctuated two
// different ways. The sign lives in the client's own template: where it puts a
// "+" the value is meant to read as a gain, so a negative value drops that "+"
// instead of printing "+-10".
function formatApply(type, val, lg) {
  var meta = APPLY_META[type];
  if (!meta) return 'Bonus #' + type + ': ' + (val > 0 ? '+' : '') + val;
  var text = meta[lg] || meta.en;
  if (meta.f === 'boolean') return text;
  var shown = (meta.f === 'multiplier' || meta.f === 'percent_decimal')
      ? (Math.round(val * 10) / 10).toFixed(1)
      : String(val);
  // The templates are C format strings, so a literal per-cent sign is
  // written %% and has to be collapsed after the value goes in - other-
  // wise every percentage line ends in "10%%" instead of "10%".
  return text.replace(/\\+?%(\\.1f|d|s)/, function(match) {
    var plus = match.charAt(0) === '+';
    if (!plus) return shown;
    return val < 0 ? shown : '+' + shown;
  }).replace(/%%/g, '%');
}

function applyIsHidden(type) { return !APPLY_META[type]; }

function skillIconImg(vnum, rank, size) {
  // A rank beginning with M, G or P means the skill is trained past normal, and
  // those have their own "_m" sprite. Not every skill got one, so a missing
  // master sprite falls back to the base icon rather than showing a broken
  // image - and a missing base icon hides the element entirely, because the
  // icons are client artwork and are not present in every installation.
  var useMaster = rank && /^[MGP]/.test(rank);
  var src = '/static/skill_icons/' + vnum + (useMaster ? '_m' : '') + '.png';
  var fallback = '/static/skill_icons/' + vnum + '.png';
  var px = size || 20;
  return '<img src="' + src + '" style="width:' + px + 'px;height:' + px +
         'px;vertical-align:middle;image-rendering:pixelated;margin-right:6px" ' +
         'onerror="if(this.src.indexOf(\\'_m\\')>=0){this.src=\\'' + fallback +
         '\\';}else{this.style.display=\\'none\\';}">';
}

var g_currentSafeboxItems = [];
var g_currentSafeboxPid = null;

// Fills the 45-cell (5x9) backdrop once. The window is static markup rather
// than something rebuilt per render, so this only ever has to run one time.
function ensureSafeboxGridCells() {
  var bg = document.getElementById('m2SafeboxGridBg');
  if (!bg || bg.children.length > 0) return;
  var html = '';
  for (var c = 0; c < 45; c++) html += '<div class="m2-grid-cell"></div>';
  bg.innerHTML = html;
}

function renderSafeboxGrid(items) {
  var overlay = document.getElementById('m2SafeboxItemOverlay');
  if (!overlay) return;
  overlay.innerHTML = '';
  items.forEach(function(it) {
    if (it.pos < 0 || it.pos >= 45) return;
    var col = it.pos % 5;
    var row = Math.floor(it.pos / 5);
    var def = (g_itemDefs && g_itemDefs[String(it.vnum)]) || {};
    var size = def.size || 1;
    if (size < 1) size = 1;
    if (size > 3) size = 3;

    var el = document.createElement('div');
    el.className = 'm2-item-entity';
    el.style.left = (col * 34) + 'px';
    el.style.top = (row * 34) + 'px';
    el.style.width = '34px';
    el.style.height = (size * 34) + 'px';

    var img = document.createElement('img');
    img.src = getItemIconUrl(it.vnum);
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
  var empty = document.getElementById('m2SafeboxEmpty');
  if (empty) empty.style.display = items.length ? 'none' : 'block';
}

function toggleBotSafeboxFromEl(el) {
  var pid = parseInt(el.getAttribute('data-botpid'), 10);
  toggleBotSafebox(pid, el.getAttribute('data-botname'));
}

function closeSafeboxWindow() {
  var win = document.getElementById('m2SafeboxWindow');
  if (win) win.style.display = 'none';
  g_currentSafeboxPid = null;
}

function toggleBotSafebox(pid, name) {
  var win = document.getElementById('m2SafeboxWindow');
  if (!win) return;

  if (win.style.display !== 'none' && g_currentSafeboxPid === pid) {
    closeSafeboxWindow();
    return;
  }

  ensureSafeboxGridCells();
  win.style.display = 'block';
  g_currentSafeboxPid = pid;
  var titleEl = document.getElementById('m2SafeboxTitle');
  if (titleEl) titleEl.textContent = (I18N.depot || 'Magazyn') + (name ? ' — ' + name : '');

  renderSafeboxGrid([]);
  fetch('/api/bot_safebox/' + pid)
    .then(function(res) { return res.json(); })
    .then(function(data) {
      // The window may have been pointed at a different bot while this was in
      // flight; dropping the stale answer is cheaper than cancelling it.
      if (g_currentSafeboxPid !== pid) return;
      if (!data || !data.ok) return;
      g_currentSafeboxItems = data.items || [];
      renderSafeboxGrid(g_currentSafeboxItems);
    })
    .catch(function() {});
}

// Dragging by the header, the way the game's own windows behave. The first
// drag swaps the initial top/right anchor for an absolute left/top.
(function initSafeboxDrag() {
  var win, header, dragging = false, offsetX = 0, offsetY = 0;

  function onPointerDown(ev) {
    win = document.getElementById('m2SafeboxWindow');
    if (!win) return;
    dragging = true;
    var rect = win.getBoundingClientRect();
    var point = ev.touches ? ev.touches[0] : ev;
    offsetX = point.clientX - rect.left;
    offsetY = point.clientY - rect.top;
    win.style.left = rect.left + 'px';
    win.style.top = rect.top + 'px';
    win.style.right = 'auto';
    ev.preventDefault();
  }

  function onPointerMove(ev) {
    if (!dragging || !win) return;
    var point = ev.touches ? ev.touches[0] : ev;
    var maxLeft = window.innerWidth - win.offsetWidth;
    var maxTop = window.innerHeight - win.offsetHeight;
    win.style.left = Math.min(Math.max(0, point.clientX - offsetX), Math.max(0, maxLeft)) + 'px';
    win.style.top = Math.min(Math.max(0, point.clientY - offsetY), Math.max(0, maxTop)) + 'px';
  }

  function onPointerUp() { dragging = false; }

  header = document.getElementById('m2SafeboxHeader');
  if (header) {
    header.addEventListener('mousedown', onPointerDown);
    header.addEventListener('touchstart', onPointerDown, {passive: false});
    document.addEventListener('mousemove', onPointerMove);
    document.addEventListener('touchmove', onPointerMove, {passive: false});
    document.addEventListener('mouseup', onPointerUp);
    document.addEventListener('touchend', onPointerUp);
  }
})();

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
        if (applyIsHidden(ap.type)) return;
        statHtml += '<div class="m2-tt-stat">' + formatApply(ap.type, ap.val, lg) + '</div>';
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
      if (applyIsHidden(a.type)) return;
      html += '<div class="m2-tt-bonus">' + formatApply(a.type, a.val, lg) + '</div>';
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
      // The account decides, not the nickname: renaming a bot used to relabel
      // it GRACZ here while every other view still knew what it was.
      var isBot = !!p.is_bot;
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
          html += '<div class="skill-row"><span>' +
                  skillIconImg(skill.vnum, skill.rank, 18) + skill.name + '</span>' +
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

      // The depot is not a wear slot, so it has no entry in equipCoords. The
      // container is 200px wide and the eight real slots stop at x=132, so it
      // goes in that unused margin rather than overlapping any of them.
      html += '<div class="m2-equip-slot" title="' + (I18N.depot || 'Magazyn') +
              '" style="left:150px;top:6px;width:34px;height:34px;cursor:pointer;' +
              'display:flex;align-items:center;justify-content:center;font-size:19px"' +
              ' data-botpid="' + p.id + '" data-botname="' + p.name + '"' +
              ' onclick="toggleBotSafeboxFromEl(this)">\U0001F4E6</div>';

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
    // 'auto' lets the server pick the character that actually played last.
    // This used to send a hardcoded name, so the button teleported that one
    // player on every installation and silently did nothing for everyone else.
    body: JSON.stringify({ x: x, y: y, player_name: 'auto' })
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

# ---------------------------------------------------------------------------
# What makes a character a bot, in one place instead of eighteen.
#
# The name used to be the test: everything this project creates is called
# bot<something>, so `name LIKE 'bot%'` found them all. Rename them and the live
# map, all eleven rankings, the world statistics and the season page stop
# counting them - and an operator who had renamed his own was re-editing this
# file by hand after every update to get them back.
#
# The core never asks the name. CPlayerBotManager::LoadRegisteredBots accepts a
# character only when its account login is exactly playerbot_NNN, and a rename
# does not touch an account login. So that is what is asked here too, with the
# old name test kept beside it: a hand-made bot on an ordinary account has
# always been visible in this panel and stays visible.
#
# Queries carry a marker rather than the predicate, because the predicate has to
# be spelled four ways - with or without the table alias the query happens to
# use, and with % doubled or not depending on whether that query passes
# parameters, since only those go through %-formatting. The marker is expanded
# here; nothing from a request is ever interpolated, and every value still
# travels as a parameter exactly as before.
# ---------------------------------------------------------------------------
def _bot_identity(alias, pct):
    ref = (alias + ".") if alias else ""
    return ("(EXISTS (SELECT 1 FROM account.account ba"
            " WHERE ba.id = " + ref + "account_id"
            " AND LEFT(ba.login, 10) = 'playerbot_')"
            " OR " + ref + "name LIKE 'bot" + pct + "')")


_BOT_MARKERS = {
    "<<BOT_2>>":     _bot_identity("", "%%"),
    "<<BOT_1>>":     _bot_identity("", "%"),
    "<<BOT_P_2>>":   _bot_identity("p", "%%"),
    "<<BOT_P_1>>":   _bot_identity("p", "%"),
    "<<BOT_NOT_1>>": "NOT " + _bot_identity("", "%"),
}


def bot_sql(query):
    """Expand the bot-identity markers in a query."""
    for marker, predicate in _BOT_MARKERS.items():
        if marker in query:
            query = query.replace(marker, predicate)
    return query


@app.route("/api/admin/warp_me", methods=["POST"])
def api_admin_warp_me():
    try:
        data = request.get_json(force=True, silent=True) or {}
        target_x = int(data.get("x", 0))
        target_y = int(data.get("y", 0))
        gm_name = data.get("player_name") or "auto"

        # Fallback to the latest active human player
        if gm_name == "auto":
            with db() as c, c.cursor() as cur:
                cur.execute(bot_sql("SELECT name FROM player.player WHERE <<BOT_NOT_1>> ORDER BY last_play DESC LIMIT 1"))
                r = cur.fetchone()
                if not r:
                    # Naming a character that may not exist would queue a command
                    # nothing ever answers, and the caller would be told the
                    # teleport succeeded. Say what is actually wrong instead.
                    return jsonify({"ok": False, "error": "no_human_player"}), 404
                gm_name = r["name"]

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


# A terrain backdrop for each map, drawn from the same server_attr the bots
# navigate by rather than captured from the client: land, river, blocked ground
# and crossings in four colours, 512x512, a few kilobytes each. What a viewer
# sees is therefore exactly what the bots can walk on - a river on this picture
# is a river the markers will never cross - which makes it a diagnostic as much
# as a backdrop.
#
# The gold threads are the crossings: water with the block bit cleared, which is
# how the map data spells a bridge deck or a ford. Orc Valley is twenty-three
# islands joined by twenty-two of them, and until the navigation learned to read
# that distinction the bots stood on the one island the entrance opens onto
# while this picture showed a delta with no way over. Both read the same file
# now. Regenerated with tools/render_map_tiles.py when a map is added.
PLAYERBOT_MAP_TILES = {
    21: (
        "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"
        "QmCC"
    ),
    23: (
        "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"
        "Op6itMJMUsGk09/UQwihv0PS6SBTmistmFvm1pTNOo5lV//NAgIgsXc/R7sY3/XakisC7JaTOy4+u9MMGQBAse2sifRaVjDL"
        "PLy/mUJpedoY1mxgwFme1xw3RARJFhDiIjV0d1sgjgMtTBrpaGJj4p4Myn8oXB8dkeYDLMmbqVIWf5hGvnW9YSraDFni9OtJ"
        "AFBvmBriTOrsBQDQidm8NCOPcGpLAOva+brR8po5hHSyAe6d2BgtyoOfNLQEhACAeTLaBwCAPA8rAUHYhX6MFuv6SUAJCLKB"
        "QuYF1mWo4Dc8A1ILo80iBn/BGeDlRdpYC47j7SFCj/XCH18eMY+dWNzXvxbnifS6se496qvobHrDHrdJkwHgdkztA7xJE6jX"
        "HrL922RA7fgiv5U/IQZUSwYj78YxJTfIdhD/FeMpMWAc1GEGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAG"
        "YAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAG"
        "YAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAGYAZgBmAG"
        "YAbgBVz/H236qZ7/0KipAAAAAElFTkSuQmCC"
    ),
    24: (
        "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"
        "AAAAAADQ1/Z//oAcVzW5M+8AAAAASUVORK5CYII="
    ),
    25: (
        "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"
        "YII="
    ),
    63: (
        "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"
        "/2I2hxy4gPRKAAAAAElFTkSuQmCC"
    ),
    64: (
        "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"
        "AABJRU5ErkJggg=="
    ),
}


# The game core already writes every death and every destroyed Metin to
# log.log with world coordinates, so the heatmap needs no extra bookkeeping in
# the server. Rows carry no map index, but the maps occupy disjoint rectangles
# of the world, so the bounding box identifies the map exactly.
PLAYERBOT_HEATMAP_KINDS = {
    "deaths": "DEAD_BY_NPC",
    "metins": "STONE_KILL",
    # Where the bots got better rather than where they died. Skill-ups are the
    # densest progression event the engine logs - 31k of them against 64k
    # deaths - so this layer reads as "where the hours went".
    "skills": "SKILLUP",
}
# 72 rather than 44. A kingdom map is 102400 units across, so a cell is now
# 1422 units - about the distance a bot can see - and a hotspot lands on the
# building it happened at instead of on the district. The count of cells this
# can return is bounded by the events in them, not by the grid.
PLAYERBOT_HEATMAP_GRID = 72


@app.route("/api/map_tile/<int:map_index>")
def api_map_tile(map_index):
    encoded = PLAYERBOT_MAP_TILES.get(map_index)
    if not encoded:
        return ("", 404)
    payload = base64.b64decode("".join(encoded))
    response = app.response_class(payload, mimetype="image/png")
    # The tile only changes when the map data does, which is never at runtime.
    response.headers["Cache-Control"] = "public, max-age=604800"
    return response


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
            # it off the live map. The <<BOT_1>> arm keeps any legacy or
            # hand-made bot visible too.
            cur.execute(bot_sql("""
                SELECT p.id, p.name, p.level, p.job, p.x, p.y, p.hp, p.gold, p.map_index
                FROM player.player p
                LEFT JOIN account.account a ON a.id = p.account_id
                WHERE (LEFT(a.login, 10) = 'playerbot_' OR <<BOT_P_1>>)
                  AND p.map_index IN (21, 23, 24, 25, 63, 64)
                ORDER BY p.level DESC, p.id ASC
            """))
            rows = cur.fetchall()
            bots = []
            # The map is the live world, not the roster. Only a fraction of the
            # characters spawn (350 of 668 here), and the rest keep whatever
            # position they were last saved at - which used to draw 223 bots
            # standing in Joan while the game held two. A row with no live entry
            # is a bot that is not in the world, so it does not belong on a map
            # of the world.
            #
            # If the status file is missing entirely the server is not running,
            # and falling back to saved positions is better than an empty map.
            trust_live = len(live_status) > 0
            total_count = 0
            for r in rows:
                pid = r.get("id") or 0
                live = live_status.get(int(pid))
                if trust_live and not live:
                    continue
                total_count += 1
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
            cur.execute(bot_sql("""
                SELECT id, name, level, job, exp, gold, hp, mp, x, y,
                       horse_level, st, ht, dx, iq, stat_point, skill_point,
                       skill_group, skill_level,
                       <<BOT_2>> AS is_bot
                FROM player.player
                WHERE id = %s
            """), (pid,))
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

@app.route("/api/bot_safebox/<int:pid>")
def api_bot_safebox(pid):
    # Safebox rows are not owned by the character's pid the way inventory and
    # equipment rows are. CSafebox belongs to the ACCOUNT - a real player can
    # check an item in on one character and take it out on another - so
    # item.owner_id here is player.account_id. Same table, same columns,
    # different owner.
    language = lang()
    try:
        with db() as c, c.cursor() as cur:
            cur.execute("SELECT account_id FROM player.player WHERE id = %s", (pid,))
            row = cur.fetchone()
            if not row:
                return jsonify({"ok": False, "error": "not found"}), 404
            cur.execute(
                """
                SELECT id, pos, `count`, vnum, socket0, socket1, socket2,
                       attrtype0, attrvalue0, attrtype1, attrvalue1,
                       attrtype2, attrvalue2, attrtype3, attrvalue3,
                       attrtype4, attrvalue4, attrtype5, attrvalue5,
                       attrtype6, attrvalue6
                  FROM player.item
                 WHERE owner_id = %s AND `window` = 'SAFEBOX'
                 ORDER BY pos ASC
                """,
                (row["account_id"],),
            )
            items = []
            for it in cur.fetchall():
                vnum = it.get("vnum") or 0
                attrs = []
                for a_idx in range(7):
                    atype = it.get("attrtype%d" % a_idx) or 0
                    aval = it.get("attrvalue%d" % a_idx) or 0
                    if atype != 0 and aval != 0:
                        attrs.append({"type": atype, "val": aval})
                items.append({
                    "id": it.get("id"),
                    "vnum": vnum,
                    "name": localized_item_name(vnum, language),
                    "count": it.get("count") or 1,
                    "pos": it.get("pos") or 0,
                    "sockets": [it.get("socket0") or 0, it.get("socket1") or 0,
                                it.get("socket2") or 0],
                    "attrs": attrs,
                })
            return jsonify({"ok": True, "items": items})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 500

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
                cur.execute(bot_sql("""
                    SELECT id, name, level, job, gold
                    FROM player.player
                    WHERE <<BOT_2>>
                    ORDER BY gold DESC, level DESC
                    LIMIT %s
                """), (rank_limit,))
            elif rtype == "weapon30":
                cur.execute(bot_sql("""
                    SELECT p.id, p.name, p.level, p.job, p.gold,
                           i.vnum as weapon_vnum, i.window as item_window,
                           i.attrtype0, i.attrvalue0, i.attrtype1, i.attrvalue1,
                           i.attrtype2, i.attrvalue2, i.attrtype3, i.attrvalue3,
                           i.attrtype4, i.attrvalue4, i.attrtype5, i.attrvalue5,
                           i.attrtype6, i.attrvalue6
                    FROM player.item i
                    JOIN player.player p ON p.id = i.owner_id
                    WHERE <<BOT_P_2>> AND (
                        (i.vnum BETWEEN 290 AND 299) OR
                        (i.vnum BETWEEN 1170 AND 1179) OR
                        (i.vnum BETWEEN 2150 AND 2159) OR
                        (i.vnum BETWEEN 3210 AND 3219) OR
                        (i.vnum BETWEEN 5110 AND 5119) OR
                        (i.vnum BETWEEN 7160 AND 7169)
                    )
                    ORDER BY i.id DESC
                    LIMIT %s
                """), (rank_limit,))
            elif rtype == "weapon":
                cur.execute(bot_sql("""
                    SELECT p.id, p.name, p.level, p.job, p.gold, i.vnum as weapon_vnum
                    FROM player.player p
                    LEFT JOIN player.item i ON p.id = i.owner_id AND i.window = 'EQUIPMENT' AND i.pos = 4
                    WHERE <<BOT_P_2>>
                    ORDER BY MOD(i.vnum, 10) DESC, i.vnum DESC, p.level DESC
                    LIMIT %s
                """), (rank_limit,))
            elif rtype == "armor":
                cur.execute(bot_sql("""
                    SELECT p.id, p.name, p.level, p.job, p.gold, i.vnum as armor_vnum
                    FROM player.player p
                    LEFT JOIN player.item i ON p.id = i.owner_id AND i.window = 'EQUIPMENT' AND i.pos = 0
                    WHERE <<BOT_P_2>>
                    ORDER BY MOD(i.vnum, 10) DESC, i.vnum DESC, p.level DESC
                    LIMIT %s
                """), (rank_limit,))
            elif rtype == "items":
                cur.execute(bot_sql("""
                    SELECT p.id, p.name, p.level, p.job, p.gold, COUNT(i.id) as item_count
                    FROM player.player p
                    LEFT JOIN player.item i ON p.id = i.owner_id AND i.window = 'INVENTORY'
                    WHERE <<BOT_P_2>>
                    GROUP BY p.id
                    ORDER BY item_count DESC, p.level DESC
                    LIMIT %s
                """), (rank_limit,))
            elif rtype == "horse":
                cur.execute(bot_sql("""
                    SELECT id, name, level, job, gold, horse_level
                    FROM player.player
                    WHERE <<BOT_2>>
                    ORDER BY horse_level DESC, level DESC, exp DESC
                    LIMIT %s
                """), (rank_limit,))
            elif rtype == "biologist":
                mission_names = tuple(m[0] for m in BIOLOGIST_MISSIONS)
                placeholders = ",".join(["%s"] * len(mission_names))
                ranking_sql = bot_sql("""
                    SELECT p.id, p.name, p.level, p.job, p.gold,
                           COUNT(DISTINCT CASE
                               WHEN q.szState = '__status' AND q.lValue = %s
                               THEN q.szName END) AS biologist_completed
                    FROM player.player p
                    LEFT JOIN player.quest q
                      ON q.dwPID = p.id AND q.szName IN ({})
                    WHERE <<BOT_P_2>>
                    GROUP BY p.id
                    ORDER BY biologist_completed DESC, p.level DESC, p.exp DESC
                    LIMIT %s
                """).format(placeholders)
                cur.execute(ranking_sql, (BIOLOGIST_COMPLETE_STATE,) + mission_names +
                            (rank_limit,))
            elif rtype == "hunting":
                cur.execute(bot_sql("""
                    SELECT p.id, p.name, p.level, p.job, p.gold,
                           MAX(CASE WHEN q.szState = 'complete' THEN q.lValue ELSE 0 END) AS hunting_complete,
                           MAX(CASE WHEN q.szState = 'current' THEN q.lValue ELSE 0 END) AS hunting_current,
                           MAX(CASE WHEN q.szState = 'select' THEN q.lValue ELSE 1 END) AS hunting_select,
                           MAX(CASE WHEN q.szState = 'remain' THEN q.lValue ELSE 0 END) AS hunting_remain
                    FROM player.player p
                    LEFT JOIN player.quest q
                      ON q.dwPID = p.id AND q.szName = 'levelup'
                    WHERE <<BOT_P_2>>
                    GROUP BY p.id
                    ORDER BY hunting_complete DESC, hunting_current DESC,
                              hunting_remain ASC, p.level DESC
                    LIMIT %s
                """), (rank_limit,))
            elif rtype == "shops":
                # An open stall exists only in the game core's memory, so this is
                # the one ranking the database cannot answer. The live status file
                # can: a keeper reports BOT_ACTION_STALL for as long as its stall
                # stands, which is what separates it from a bot merely visiting a
                # merchant (BOT_ACTION_SHOP).
                live = read_playerbot_live_status()
                keeper_ids = [pid for pid, entry in live.items()
                              if entry.get("action_id") == BOT_ACTION_STALL_ID]
                if not keeper_ids:
                    return jsonify({"ok": True, "type": rtype, "limit": rank_limit,
                                    "rankings": []})
                keeper_ids = keeper_ids[:rank_limit]
                placeholders = ",".join(["%s"] * len(keeper_ids))
                cur.execute(("""
                    SELECT id, name, level, job, gold
                    FROM player.player
                    WHERE id IN ({})
                    ORDER BY level DESC
                """).format(placeholders), tuple(keeper_ids))
            elif rtype == "skills":
                cur.execute(bot_sql("""
                    SELECT id, name, level, job, gold, skill_group, skill_level
                    FROM player.player
                    WHERE <<BOT_2>> AND skill_group > 0
                    ORDER BY level DESC
                    LIMIT 400
                """))
            elif rtype == "plus9":
                # Equipment stores its refine in the vnum: base + 0..9. Anything
                # below 12000 is wearable; the tables above that are materials and
                # consumables, whose vnums ending in 9 mean nothing of the sort.
                cur.execute(bot_sql("""
                    SELECT p.id, p.name, p.level, p.job, p.gold,
                           i.vnum as weapon_vnum, i.window as item_window
                    FROM player.item i
                    JOIN player.player p ON p.id = i.owner_id
                    WHERE <<BOT_P_2>> AND i.vnum < 12000
                      AND MOD(i.vnum, 10) = 9
                    ORDER BY i.vnum DESC, p.level DESC
                    LIMIT %s
                """), (rank_limit,))
            else: # level
                cur.execute(bot_sql("""
                    SELECT id, name, level, job, exp, gold
                    FROM player.player
                    WHERE <<BOT_2>>
                    ORDER BY level DESC, exp DESC
                    LIMIT %s
                """), (rank_limit,))

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

                # Only the skills ranking pays to decode the packed skill
                # table; every other ranking would be doing it for nothing. What
                # counts is the single highest grade a bot has reached - "M10
                # Silne Ciało" - not how much it has spread across the tree.
                skill_score = 0
                skill_rank = ""
                skill_name = ""
                if rtype == "skills":
                    for entry in parse_player_skills(r.get("skill_level"),
                                                     r.get("job"),
                                                     r.get("skill_group"), language):
                        score = skill_rank_score(entry.get("master_type"),
                                                 entry.get("level"))
                        if score > skill_score:
                            skill_score = score
                            skill_rank = entry.get("rank") or ""
                            skill_name = entry.get("name") or ""
                stall_map = ""
                if rtype == "shops":
                    entry = live.get(r["id"]) or {}
                    stall_map = messages.get(
                        {21: "m1", 23: "m2", 24: "m3"}.get(entry.get("map_index"), ""), "")

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
                    "skill_rank": skill_rank,
                    "skill_rank_name": skill_name,
                    "skill_score": skill_score,
                    "stall_map": stall_map,
                    "in_pt": bot_in_party_cohort(r["id"])
                })

            if rtype == "weapon30":
                rankings.sort(key=lambda x: (x["sr"], x["um"], x["level"]), reverse=True)
            elif rtype == "skills":
                # Ordered here rather than in SQL: the grade comes out of the
                # packed skill table, which MySQL cannot read.
                rankings.sort(key=lambda x: (x["skill_score"], x["level"]), reverse=True)
                rankings = rankings[:rank_limit]

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


@app.route("/ai", methods=["GET", "POST"])
@login_required
def ai_weights():
    """The goal weights the running game core reads for itself.

    Unlike the rates page there is nothing to restart and no helper to wake:
    the core stats this file every five seconds and applies it on the next
    planning tick, so saving is the whole operation.
    """
    if request.method == "POST":
        # (the global before_request hook has already checked the CSRF token)
        vals = {}
        for name, _ in AI_WEIGHT_KEYS:
            try:
                v = int(request.form.get(name, AI_W_NEUTRAL))
            except (TypeError, ValueError):
                v = AI_W_NEUTRAL
            # Clamped rather than rejected. A slider cannot send anything out of
            # range, so a value that is out of range came from somewhere else and
            # the sane answer is the nearest legal one, not an error page.
            vals[name] = max(AI_W_MIN, min(AI_W_MAX, v))
        try:
            write_ai_weights(vals)
        except OSError:
            flash(t("ai_failed"), "error")
            return redirect(url_for("ai_weights"))
        flash(t("ai_live"))
        return redirect(url_for("ai_weights"))

    return render_template_string(TPL_AI, cur=read_ai_weights(),
                                  keys=AI_WEIGHT_KEYS, wmin=AI_W_MIN,
                                  wmax=AI_W_MAX, wneutral=AI_W_NEUTRAL)



# =============================================================================
#  The weekly season, and the server records.
# =============================================================================
#
# Nothing here is counted by the game core, and nothing needs to be: the server
# has been writing every metin break, every boss, every successful refine and
# every death into log.log with the character id since the world was created.
# The season is therefore a question asked of data that already exists, which is
# why it works backwards over the whole history instead of starting on the day
# the feature shipped.
#
# What is deliberately NOT scored is everything that is a state rather than an
# event: a level, a horse, the gold in the bag. Those cannot be attributed to
# the last seven days without a snapshot nobody took, and scoring them would
# rank a bot this week for what it did in August. They sit beside the score as
# context, and they are what the all-time records are made of.

SEASON_DAYS = 7
SEASON_CACHE_SECONDS = 60

# What each event is worth. Metins are the common currency; a boss is worth a
# morning of them, and a refine that lands on +7 or better is rarer than either.
SEASON_POINTS_METIN = 150
SEASON_POINTS_BOSS = 500
SEASON_POINTS_REFINE = 200

# The plus levels that count as an achievement rather than a Tuesday. The log
# records the item name with its new level appended -- "Bojowa Tarcza+4" -- so
# this is the one place the panel reads a name to learn a number.
SEASON_REFINE_PATTERN = "[+][789]$"

_season_cache = {"at": 0.0, "data": None}


def _season_counts(cur, how, since, extra=""):
    """One event kind, per character, inside the window."""
    cur.execute(
        "SELECT who, COUNT(*) AS n FROM log.log "
        "WHERE how = %s AND time >= %s " + extra + " GROUP BY who",
        (how, since))
    return {int(r["who"]): int(r["n"]) for r in cur.fetchall()}


def season_data():
    """The season table and the all-time records, cached for a minute.

    Cached because the refine query has to look through a third of a million
    rows to find the handful that landed on +7, and the answer does not change
    quickly enough for anyone to notice a minute of staleness.
    """
    now = time.time()
    if (_season_cache["data"] is not None
            and now - _season_cache["at"] < SEASON_CACHE_SECONDS):
        return _season_cache["data"]

    since = (datetime.datetime.now()
             - datetime.timedelta(days=SEASON_DAYS)).strftime("%Y-%m-%d %H:%M:%S")
    out = {"rows": [], "records": {}, "days": SEASON_DAYS, "error": ""}
    try:
        with db() as c, c.cursor() as cur:
            metins = _season_counts(cur, "STONE_KILL", since)
            bosses = _season_counts(cur, "BOSS_KILL", since)
            refines = _season_counts(cur, "REFINE SUCCESS", since,
                                     "AND hint REGEXP '" + SEASON_REFINE_PATTERN + "' ")
            deaths = _season_counts(cur, "DEAD_BY_NPC", since)

            # Only the characters that did something this week need naming: a
            # world with a thousand bots must not fetch a thousand rows to show
            # fifty of them.
            active = set(metins) | set(bosses) | set(refines)
            rows = []
            if active:
                cur.execute(
                    bot_sql("SELECT id, name, level, job, horse_level FROM player.player "
                    "WHERE <<BOT_2>> AND id IN %s"), (tuple(sorted(active)),))
                for p in cur.fetchall():
                    pid = int(p["id"])
                    m = metins.get(pid, 0)
                    b = bosses.get(pid, 0)
                    r = refines.get(pid, 0)
                    rows.append({
                        "pid": pid,
                        "name": p["name"],
                        "level": int(p["level"] or 0),
                        "horse": int(p["horse_level"] or 0),
                        "metins": m, "bosses": b, "refines": r,
                        "deaths": deaths.get(pid, 0),
                        "score": (m * SEASON_POINTS_METIN + b * SEASON_POINTS_BOSS
                                  + r * SEASON_POINTS_REFINE),
                    })
            rows.sort(key=lambda x: (-x["score"], -x["metins"], x["name"]))
            out["rows"] = rows[:50]

            # --- the all-time records --------------------------------------
            # Every tile is {name, level}, whatever "level" means for that tile,
            # so the template does not need a branch per record.
            recs = {}
            cur.execute(bot_sql("SELECT name, level FROM player.player "
                        "WHERE <<BOT_1>> ORDER BY level DESC, exp DESC LIMIT 1"))
            recs["level"] = cur.fetchone()
            cur.execute(bot_sql("SELECT name, horse_level AS level FROM player.player "
                        "WHERE <<BOT_1>> ORDER BY horse_level DESC LIMIT 1"))
            recs["horse"] = cur.fetchone()
            cur.execute(bot_sql("SELECT name, gold AS level FROM player.player "
                        "WHERE <<BOT_1>> ORDER BY gold DESC LIMIT 1"))
            recs["gold"] = cur.fetchone()
            for key, how, extra in (
                    ("metins", "STONE_KILL", ""),
                    ("bosses", "BOSS_KILL", ""),
                    ("refines", "REFINE SUCCESS",
                     "AND l.hint REGEXP '" + SEASON_REFINE_PATTERN + "' ")):
                cur.execute(
                    bot_sql("SELECT p.name, COUNT(*) AS level FROM log.log l "
                    "JOIN player.player p ON p.id = l.who "
                    "WHERE l.how = %s AND <<BOT_P_2>> ") + extra +
                    "GROUP BY l.who ORDER BY level DESC LIMIT 1", (how,))
                recs[key] = cur.fetchone()
            out["records"] = {k: v for k, v in recs.items() if v}
    except Exception as e:
        out["error"] = str(e)

    _season_cache["at"] = now
    _season_cache["data"] = out
    return out


@app.route("/season")
def season():
    """Public on purpose: this is the page an operator links to, not an admin tool."""
    return render_template_string(TPL_SEASON, s=season_data())

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
