import json
import os
import hmac
import socket
import time
import re
import uuid
from pathlib import Path
from urllib.parse import quote
from urllib.request import Request, urlopen
from urllib.error import URLError, HTTPError
from datetime import datetime, timedelta
from functools import wraps

import pymysql
from flask import Flask, abort, flash, redirect, render_template, request, session, url_for
from markupsafe import escape
from werkzeug.security import check_password_hash, generate_password_hash

app = Flask(__name__)
app.secret_key = os.environ.get("SEBAN_SESSION_SECRET", "change-this-before-public-use")
app.config.update(
    SESSION_COOKIE_HTTPONLY=True, SESSION_COOKIE_SAMESITE="Lax",
    # Cookies are scoped to the host, not the port. A dedicated name prevents
    # the classic Tieru panel on :7788 from overwriting this panel on :7789.
    SESSION_COOKIE_NAME=os.environ.get("SEBAN_SESSION_COOKIE_NAME", "seban_panel_session"),
    PERMANENT_SESSION_LIFETIME=timedelta(days=30),
)

# Nazwy wiosek pochodzą z questów silnika: new_quest_lv52 czyta pierwsze
# wioski jako { "Yongan", "Joan", "Pyongmoo" } wg królestwa, a new_quest_lv7
# nazywa drugie Jayang, Bokjung i Bakra.
MAP_NAMES = {
    1: "Shinsoo M1 — Yongan", 3: "Shinsoo M2 — Jayang", 4: "Shinsoo M3 — Jungrang",
    5: "Loch Małp Shinsoo", 44: "Jinno M3 — Imha", 45: "Loch Małp Jinno",
    21: "Chunjo M1 — Joan", 23: "Chunjo M2 — Bokjung",
    24: "Chunjo M3 — Waryong", 25: "Łatwy Loch Małp",
    41: "Jinno M1 — Pyongmoo", 43: "Jinno M2 — Bakra",
    61: "Góra Sohan", 63: "Pustynia Yongbi", 64: "Dolina Orków", 104: "Loch Pająków V1",
    65: "Świątynia Hwang", 71: "Loch Pająków V2",
    108: "Loch Małp Normalny", 109: "Loch Małp Trudny",
    67: "Las", 68: "Czerwony Las", 66: "Wieża Demonów",
}
MAP_BOUNDS = {
    1: (409600, 896000, 102400, 128000), 3: (307200, 819200, 102400, 102400),
    4: (128000, 0, 51200, 51200), 5: (768000, 435200, 76800, 76800),
    41: (921600, 204800, 102400, 128000), 43: (819200, 204800, 102400, 102400),
    44: (230400, 0, 51200, 51200), 45: (921600, 435200, 76800, 76800),
    21: (0, 102400, 102400, 128000), 23: (102400, 204800, 102400, 102400),
    24: (179200, 0, 51200, 51200), 25: (844800, 435200, 76800, 76800),
    61: (358400, 153600, 153600, 153600), 63: (204800, 486400, 153600, 153600),
    64: (256000, 665600, 153600, 153600), 104: (51200, 486400, 76800, 76800),
    65: (537600, 51200, 102400, 102400), 71: (665600, 435200, 102400, 102400),
    108: (128000, 640000, 76800, 76800), 109: (128000, 716800, 76800, 76800),
    # Re-derived 2026-09-15 straight from each map's own Setting.txt
    # (BasePosition + MapSize x 25600, the same formula that reproduces
    # Chunjo M1's already-correct (0,102400,102400,128000) from its own
    # MapSize 4x5 / BasePosition 0,102400) -- the original values here were
    # wrong on all three axes for at least one of the three maps each,
    # flagged by Tieru testing the exported panel.
    67: (281600, 0, 51200, 51200), 68: (1049600, 0, 76800, 76800),
    66: (128000, 793600, 76800, 76800),
}
TRACKED_MAP_OPTIONS = tuple((index, MAP_NAMES[index]) for index in MAP_BOUNDS)
MAP_RESPAWN_OPTIONS = (
    (1, "Shinsoo M1 — Yongan"), (3, "Shinsoo M2 — Jayang"), (21, "Chunjo M1 — Joan"),
    (23, "Chunjo M2 — Bokjung"), (41, "Jinno M1 — Pyongmoo"), (43, "Jinno M2 — Bakra"),
    (4, "Shinsoo M3 — Jungrang"), (24, "Chunjo M3 — Waryong"), (44, "Jinno M3 — Imha"),
    (5, "Loch Małp Shinsoo"), (45, "Loch Małp Jinno"),
    (25, "Łatwy Loch Małp"), (61, "Góra Sohan"), (63, "Pustynia Yongbi"), (64, "Dolina Orków"),
    (104, "Loch Pająków V1"), (71, "Loch Pająków V2"), (108, "Loch Małp Normalny"), (109, "Loch Małp Trudny"),
)
# Monkey Dungeons and Spider Dungeon V1 ship no stone.txt, so only their mob
# respawns can be configured. The explicit allowlist also protects the helper.
MAP_STONE_RESPAWN_IDS = frozenset(index for index, _name in MAP_RESPAWN_OPTIONS if index not in {5, 25, 45, 104, 71, 108, 109})
STATUS_GLOBS = (os.environ.get("PLAYERBOTS_STATUS_GLOB", "/opt/metin2/var/channel*/*/playerbot_status.tsv"),)
GUILD_STATUS_GLOBS = (os.environ.get("PLAYERBOTS_GUILD_STATUS_GLOB", "/opt/metin2/var/channel1/*/playerbot_guild_status.tsv"),)
GUILD_TIERS = {0: "Elitarna", 1: "Silna", 2: "Średnia", 3: "Zwykła"}
BOT_SYSLOG_GLOB = os.environ.get("PLAYERBOTS_SYSLOG_GLOB", "/opt/metin2/var/channel*/*/syslog")
RATES_SPOOL = Path("/opt/m2spool")
UPDATE_SPOOL = Path("/opt/m2update")
UPDATE_WATCHER_MAX_AGE_SECONDS = 90
PLAYERBOTS_RELEASE_URL = "https://api.github.com/repos/TieruYT/metin2-playerbots/releases/latest"
PLAYERBOTS_RELEASE_CACHE_SECONDS = 900
_playerbots_release_cache = {"checked_at": 0.0, "latest": None, "error": None}
SERVER_SETTINGS_READY_MAX_AGE_SECONDS = 20
SERVER_SETTINGS_STALE_SECONDS = 600
GAME_HOST = os.environ.get("PLAYERBOTS_GAME_HOST", "metin2-game")
GAME_LOGIN_PORT = int(os.environ.get("PLAYERBOTS_LOGIN_PORT", "11000"))
GAME_WORLD_PORT = int(os.environ.get("PLAYERBOTS_WORLD_PORT", "13000"))
RATE_NAMES = ("exp", "drop", "yang")
AI_WEIGHTS_FILE = RATES_SPOOL / "playerbot_weights.tsv"
AI_WEIGHT_KEYS = (
    ("RESTOCK", "Mikstury", "🧪"), ("REFINE", "Kowal", "🔨"),
    ("SKILL", "Księgi umiejętności", "📖"), ("HORSE", "Koń", "🐎"),
    ("BIOLOG", "Biolog", "🧬"), ("METIN", "Metiny", "🗿"),
    ("PARTY", "Grupy", "👥"), ("HUNTING", "Misje polowania", "🏹"),
    ("LEVEL", "Bicie potworów", "⚔️"), ("FISHING", "Wędkowanie", "🎣"),
    ("TRADE", "Stragany", "🏪"),
)
AI_WEIGHT_MIN, AI_WEIGHT_MAX, AI_WEIGHT_NEUTRAL = 25, 250, 100
# These values share the live weight file with goal weights, but the core treats
# them as switches or direct settings rather than 25–250% goal weights.
AI_LIVE_DEFAULTS = {"CHAT": 1, "BOOKS": 1, "NIGHT": 1, "LIFE": 0, "WARS": 1, "ISHOP": 1, "SCRAP": 0, "REST": 100, "CHEST": None, "CHEST_STONE": None}
AI_SPECIAL_WEIGHT_KEYS = frozenset(AI_LIVE_DEFAULTS)
BIOLOGIST_COMPLETE_STATE = 557528158
# Tieru 1.29.10 adds the Orc Tooth task after the six classic Biologist
# missions. The database lookup below also discovers future missions as soon
# as the game has created their quest rows, while this list keeps the complete
# progress scale correct before anyone has started a new task.
BIOLOGIST_FALLBACK_MISSIONS = (
    "make_herb_lv4", "make_herb_lv7", "make_herb_lv10", "make_herb_lv15",
    "make_herb_lv20", "make_herb_lv25", "collect_quest_lv30",
)
PANEL_VERSION_FILE = Path(__file__).parent / "VERSION"
GM_JOB_OPTIONS = ((0, "Wojownik"), (1, "Ninja"), (2, "Sura"), (3, "Szaman"))
# Race IDs are stored in player.job.  0–3 retain the classic class/gender
# pair; IDs 4–7 are the alternate client portraits and character models.
GM_GENDER_OPTIONS = (("classic", "Klasyczna dla klasy"), ("male", "Mężczyzna"), ("female", "Kobieta"))
GM_RACE_BY_CLASS_GENDER = {
    (0, "classic"): 0, (1, "classic"): 1, (2, "classic"): 2, (3, "classic"): 3,
    (0, "male"): 0, (0, "female"): 4,
    (1, "male"): 5, (1, "female"): 1,
    (2, "male"): 2, (2, "female"): 6,
    (3, "male"): 7, (3, "female"): 3,
}
CLASS_PROFILES = {
    0: {"name": "Wojownik", "gender": "Mężczyzna", "portrait": "warrior_m.bmp"},
    4: {"name": "Wojownik", "gender": "Kobieta", "portrait": "warrior_w.bmp"},
    1: {"name": "Ninja", "gender": "Kobieta", "portrait": "assassin_w.bmp"},
    5: {"name": "Ninja", "gender": "Mężczyzna", "portrait": "assassin_m.bmp"},
    2: {"name": "Sura", "gender": "Mężczyzna", "portrait": "sura_m.bmp"},
    6: {"name": "Sura", "gender": "Kobieta", "portrait": "sura_w.bmp"},
    3: {"name": "Szaman", "gender": "Kobieta", "portrait": "shaman_w.bmp"},
    7: {"name": "Szaman", "gender": "Mężczyzna", "portrait": "shaman_m.bmp"},
}
GM_JOB_STARTS = {0: (6, 4, 3, 3, 600, 200), 1: (4, 3, 6, 3, 650, 200), 2: (5, 3, 3, 6, 650, 200), 3: (3, 5, 3, 5, 700, 200)}
GM_EMPIRE_STARTS = {1: (469300, 964200, 1), 2: (55700, 157900, 21), 3: (969600, 278400, 41)}
GM_NAME_PATTERN = r"(?:[A-Za-z0-9_]{2,24}|\[[A-Za-z0-9_]{1,6}\][A-Za-z0-9_]{2,16})"
EMPIRES = {1: {"name": "Shinsoo", "flag": "shinsoo.png"}, 2: {"name": "Chunjo", "flag": "chunjo.png"}, 3: {"name": "Jinno", "flag": "jinno.png"}}
try:
    PANEL_VERSION = os.environ.get("SEBAN_PANEL_VERSION") or PANEL_VERSION_FILE.read_text(encoding="utf-8").strip()
except OSError:
    PANEL_VERSION = os.environ.get("SEBAN_PANEL_VERSION", "dev")
DEFAULT_SETTINGS = {
    "panel_name": "Metin2 Singleplayer", "stuck_minutes": "5", "theme": "ocean", "monitor_mode": "vps",
    # Existing installations without this key stay usable. Fresh installations
    # receive setup_complete=0 from the collector and enter the setup wizard.
    "setup_complete": "1", "auth_enabled": "0", "auth_password_hash": "", "allow_student_chest": "0", "allow_moonlight_chest": "0", "keep_demo_characters": "0", "update_seban_panel": "0",
}
try:
    ITEM_DEFS = json.loads((Path(__file__).parent / "static" / "item_defs.json").read_text(encoding="utf-8"))
except (OSError, ValueError):
    ITEM_DEFS = {}
# EPlayerBotPersonality (playerbot_types.h): MERCHANT to 5, WANDERER 6.
# Ta tabela miala 5 jako wedrowca i konczyla sie na nim, wiec straganiarz
# czytal sie jako wedrowiec, a piec dopisanych od tamtej pory osobowosci
# nie czytalo sie wcale.
BOT_PERSONALITIES = {0: "Wytrwały poszukiwacz", 1: "Pogromca Metinów", 2: "Towarzysz drużyny", 3: "Mistrz ekwipunku", 4: "Rozważny zbieracz", 5: "Handlarz", 6: "Wędrowiec", 7: "Dropek Metinów", 8: "Dropek z M3", 9: "Dropek z M2", 10: "Dropek medali"}
# Iwakura's personalities ("SYSTEM OSOBOWOSCI v2.0", playerbot_persona_rules.h,
# EPersona - the order is the interface) and the Bot Mood System's moods.
BOT_PERSONAS = {0: "Grinder", 1: "Zdobywca", 2: "Handlarz", 3: "Hazardzista", 4: "Perfekcjonista", 5: "Pogromca metinów", 6: "Górnik", 7: "Rybak", 8: "Najemnik", 9: "Towarzysz"}
BOT_MOODS = {0: "Słaby", 1: "Normalny", 2: "Bardzo dobry"}
BOT_MOOD_LOCKS = {1: "euforia po ulepszeniu", 2: "kapitulacja (Anty-PK)"}
BOT_AMBITIONS = {0: "Poziom", 1: "Ekwipunek", 2: "Metiny", 3: "Koń", 4: "Biolog", 5: "Umiejętności", 6: "Handel"}
BOT_GOALS = {0: "Zdobywanie poziomu", 1: "Przetrwanie", 2: "Wybór profesji", 3: "Zdobycie ekwipunku", 4: "Uzupełnienie zapasów", 5: "Ulepszanie EQ", 6: "Rozwój umiejętności", 7: "Polowanie na Metiny", 8: "Silne cele w PT", 9: "Misja Biologa", 10: "Misja Polowania", 11: "Rozwój konia"}
# 18 (Kopie rudę) byla dopisana do playerbot_types.h u Tieru, ale nie tutaj -
# boty kopiące rudę pokazywały gołe "#18" zamiast etykiety (audyt vs panel
# Tieru na 7788, 2026-09-14).
BOT_ACTIONS = {0: "Planuje następny ruch", 1: "Podróżuje", 2: "Walczy", 3: "Podnosi łup", 4: "Regeneruje się", 5: "Wybiera profesję", 6: "Handluje", 7: "Ulepsza EQ", 8: "Czyta KU", 9: "Wkłada KD", 10: "Organizuje PT", 11: "Robi Biologa", 12: "Odwiedza Stajennego", 13: "Prowadzi stragan", 14: "Łowi ryby", 15: "Przegląda stragany", 16: "Wabi potwory", 17: "Odpoczywa w mieście", 18: "Kopie rudę"}
# Akcje, w których bot stoi w miejscu z własnej woli: stragan, wędka, przegląd
# straganów, lada NPC, kowal, trener, odpoczynek, kopanie rudy. Bez tego każdy
# straganiarz był "Możliwie zawieszony" - a flaga z tekstu statusu łapała
# tylko wędkarzy.
STATIONARY_ACTIONS = {5, 6, 7, 13, 14, 15, 17, 18}
ITEM_TYPE_NAMES = (
    "ITEM_NONE", "ITEM_WEAPON", "ITEM_ARMOR", "ITEM_USE", "ITEM_AUTOUSE", "ITEM_MATERIAL", "ITEM_SPECIAL", "ITEM_TOOL", "ITEM_LOTTERY", "ITEM_ELK",
    "ITEM_METIN", "ITEM_CONTAINER", "ITEM_FISH", "ITEM_ROD", "ITEM_RESOURCE", "ITEM_CAMPFIRE", "ITEM_UNIQUE", "ITEM_SKILLBOOK", "ITEM_QUEST", "ITEM_POLYMORPH",
    "ITEM_TREASURE_BOX", "ITEM_TREASURE_KEY", "ITEM_SKILLFORGET", "ITEM_GIFTBOX", "ITEM_PICK", "ITEM_HAIR", "ITEM_TOTEM", "ITEM_BLEND", "ITEM_COSTUME", "ITEM_DS",
    "ITEM_SPECIAL_DS", "ITEM_EXTRACT", "ITEM_SECONDARY_COIN", "ITEM_RING", "ITEM_BELT", "ITEM_PET", "ITEM_MEDIUM", "ITEM_GACHA", "ITEM_SOUL", "ITEM_PASSIVE",
)
# Renumbered 2026-09-15: was consistently off by one or more across several
# 6-8 entry runs (18..23 "Silny przeciw", 30..39 "Odporność na", ...),
# reported as swapped bonus text on real equipped items ([GA]Seban's
# Kolczyki Z Niebiań.Łez+9 showing "Odporność na dzwony/miecze" and "Silny
# przeciw mistykom" for what the in-game tooltip calls wachlarze/broń
# dwuręczną/Nieumarłym). Re-keyed against Tieru's own APPLY_META table
# (admin_panel.py, 7788) entry by entry -- POINT_TO_APPLY below already
# matched Tieru's exactly, so only the label side was wrong. Gaps at
# 51/57/77/83 are Tieru's too (no player-visible text for those points).
APPLY_LABELS = {
    1: ("Maks. PŻ", ""), 2: ("Maks. PM", ""), 3: ("Witalność", ""), 4: ("Inteligencja", ""), 5: ("Siła", ""), 6: ("Zręczność", ""), 7: ("Szybkość ataku", "%"), 8: ("Szybkość ruchu", "%"), 9: ("Szybkość zaklęcia", "%"), 10: ("Regeneracja PŻ", "%"), 11: ("Regeneracja PM", "%"), 12: ("Szansa na otrucie", "%"), 13: ("Szansa na omdlenie", "%"), 14: ("Szansa na spowolnienie", "%"), 15: ("Szansa na cios krytyczny", "%"), 16: ("Szansa na przeszywający", "%"), 17: ("Silny przeciw ludziom", "%"), 18: ("Silny przeciw zwierzętom", "%"), 19: ("Silny przeciw orkom", "%"), 20: ("Silny przeciw mistykom", "%"), 21: ("Silny przeciw nieumarłym", "%"), 22: ("Silny przeciw diabłom", "%"), 23: ("Kradzież PŻ", "%"), 24: ("Kradzież PM", "%"), 25: ("Szansa na kradzież PM", "%"), 26: ("Odzyskanie PM po obrażeniach", "%"), 27: ("Szansa na blok", "%"), 28: ("Szansa na unik strzał", "%"), 29: ("Odporność na miecze", "%"), 30: ("Odporność na broń dwuręczną", "%"), 31: ("Odporność na sztylety", "%"), 32: ("Odporność na dzwony", "%"), 33: ("Odporność na wachlarze", "%"), 34: ("Odporność na strzały", "%"), 35: ("Odporność na ogień", "%"), 36: ("Odporność na błyskawice", "%"), 37: ("Odporność na magię", "%"), 38: ("Odporność na wiatr", "%"), 39: ("Odbicie obrażeń fizycznych", "%"), 40: ("Odbicie klątwy", "%"), 41: ("Odporność na trucizny", "%"), 42: ("Odzyskanie PM po zabiciu", "%"), 43: ("Bonus doświadczenia", "%"), 44: ("Bonus Yang", "%"), 45: ("Bonus dropu przedmiotów", "%"), 46: ("Bonus mikstur", "%"), 47: ("Odzyskanie PŻ po zabiciu", "%"), 48: ("Odporność na omdlenie", ""), 49: ("Odporność na spowolnienie", ""), 50: ("Odporność na przewrócenie", ""), 52: ("Zasięg łuku", "m"), 53: ("Wartość ataku", ""), 54: ("Wartość obrony", ""), 55: ("Wartość magicznego ataku", ""), 56: ("Magiczna wartość obrony", ""), 58: ("Maks. wytrzymałość", ""), 59: ("Silny przeciw wojownikom", "%"), 60: ("Silny przeciw ninja", "%"), 61: ("Silny przeciw surom", "%"), 62: ("Silny przeciw szamanom", "%"), 63: ("Silny przeciw potworom", "%"), 64: ("Wartość ataku", "%"), 65: ("Wartość obrony", "%"), 66: ("Bonus doświadczenia", "%"), 67: ("Szansa na zdobycie przedmiotów", ""), 68: ("Szansa na zdobycie Yang", ""), 69: ("Maks. PŻ", "%"), 70: ("Maks. PM", "%"), 71: ("Obrażenia umiejętności", "%"), 72: ("Średnie obrażenia", "%"), 73: ("Odporność na obrażenia umiejętności", "%"), 74: ("Odporność na średnie obrażenia", "%"), 75: ("Bonus doświadczenia (iCafe)", "%"), 76: ("Bonus dropu przedmiotów (iCafe)", "%"), 78: ("Odporność na wojowników", "%"), 79: ("Odporność na ninja", "%"), 80: ("Odporność na sury", "%"), 81: ("Odporność na szamanów", "%"), 82: ("Energia", ""), 84: ("Bonus kostiumu", "%"), 85: ("Magiczny atak", "%"), 86: ("Magiczny/fizyczny atak", "%"), 87: ("Odporność na lód", "%"), 88: ("Odporność na ziemię", "%"), 89: ("Odporność na mrok", "%"), 90: ("Odporność na cios krytyczny", "%"), 91: ("Odporność na przeszywający", "%"), 1138: ("Terror", "%"), 1139: ("Regeneracja wytrzymałości", "%"), 1140: ("Atak sztyletem przeciw potworom", ""), 1141: ("Wartość ataku przeciw potworom", ""), 1142: ("Odporność na potwory", "‰"), 1143: ("Pochłanianie obrażeń", "%"), 1144: ("Pochłanianie obrażeń od potworów", "%"), 1145: ("Przełamanie odporności na ogłuszenie", ""), 1146: ("Przełamanie klątwy świątyni", ""), 1147: ("Czas trwania umiejętności", "%"), 1148: ("Silny przeciw potworom z Doliny Orków", "%"), 1149: ("Silny przeciw Metinom", "%"), 1150: ("Silny przeciw bossom", "%"), 1151: ("Magiczny atak przeciw potworom", "%"), 1152: ("Przełamanie odporności na miecz", "%"), 1153: ("Przełamanie odporności na broń dwuręczną", "%"), 1154: ("Przełamanie odporności na sztylet", "%"), 1155: ("Przełamanie odporności na dzwonek", "%"), 1156: ("Przełamanie odporności na wachlarz", "%"), 1157: ("Przełamanie odporności na łuk", "%"), 1158: ("Szansa na zbieranie", "%"), 1159: ("Szansa na naukę", "%"), 1160: ("Odporność na ludzi", "%"), 1161: ("Magiczny atak", ""), 1162: ("Szansa na podpalenie", "%"), 1163: ("Zamiana obrażeń na PE", "%"), 1164: ("Szansa na rzadki łup", "%"), 1165: ("Magiczna wartość ataku przeciw potworom", ""), 1166: ("Szansa na unieruchomienie", "%"), 1167: ("Atak specjalny", ""), 1168: ("Kara za śmierć", "%")}
# 71 i 72 są w tablicy powyżej, we właściwej kolejności: common/length.h
# niesie numery we własnych komentarzach - APPLY_SKILL_DAMAGE_BONUS to 71,
# APPLY_NORMAL_HIT_DAMAGE_BONUS to 72. Stała tu wcześniej poprawka
# nadpisująca błędną tablicę i tłumacząca ją tym, że "w tej kompilacji pola
# są odwrotne" - nic ich nie odwraca. Uzasadnienie było nieprawdziwe, a samo
# nadpisanie sięgało tylko opisów przedmiotów, więc ranking - który bierze
# dane z osobnego zapytania - pokazywał je zamienione jeszcze długo potem.
# Which engine the panel looks at (PLAYERBOTS_ENGINE). mt2009 keeps an
# item's bonus lines as POINT_* numbers: the two damage lines are 121 and
# 122 there, every attrtype goes through POINT_TO_APPLY before APPLY_LABELS,
# account.account has no empire column and player.player no bank_value.
PANEL_ENGINE = os.environ.get("PLAYERBOTS_ENGINE", "r40250").strip().lower()
ENGINE_MT2009 = PANEL_ENGINE == "mt2009"
# The bag's pages as the client draws them: forty-five cells a page, two
# pages on r40250 and four on the mt2009 line since 2.0.74 (cells 90-179,
# with the horse's page moved to 180). What lies past them - the horse's
# page, the belt's cells - is no bag page, and pos % 45 drew it over page II.
INVENTORY_PAGE_SIZE = 45
INVENTORY_PAGES = 4 if ENGINE_MT2009 else 2
# Four /manage controls (target bot count, per-map respawn, student chest
# toggle, +9 refine announcements) read/write quest and wiring files this
# panel's own patch_*.py scripts (or, for +9 announcements, a hand-added
# NOTICE command in web_admin.quest) add to the managed tree -- a
# fresh/public install of this panel does not have them, so those controls
# would silently do nothing there: e.g. a queued NOTICE command would sit
# as "pending" forever with nothing compiled in to pick it up.
# Off by default (public release); this VPS's own .env turns it on since
# the patches are actually applied here. First three flagged by Tieru
# testing the exported zip on a clean install, 2026-09-15; +9 announcements
# added same day and gated the same way from the start.
CUSTOM_PATCHES_ENABLED = os.environ.get("M2_PANEL_CUSTOM_PATCHES", "0").strip().lower() in ("1", "true", "yes", "on")
ATTR_SKILL_DAMAGE = 121 if ENGINE_MT2009 else 71
ATTR_AVG_DAMAGE = 122 if ENGINE_MT2009 else 72
POINT_TO_APPLY = {6: 1, 8: 2, 13: 3, 15: 4, 12: 5, 14: 6, 17: 7, 19: 8, 21: 9, 32: 10, 33: 11,
 37: 12, 38: 13, 39: 14, 40: 15, 41: 16, 43: 17, 44: 18, 45: 19, 46: 20, 47: 21,
 48: 22, 63: 23, 64: 24, 65: 25, 66: 26, 67: 27, 68: 28, 69: 29, 70: 30, 71: 31,
 72: 32, 73: 33, 74: 34, 75: 35, 76: 36, 77: 37, 78: 38, 79: 39, 81: 41, 82: 42,
 83: 43, 84: 44, 85: 45, 86: 46, 87: 47, 88: 48, 89: 49, 90: 50, 28: 51, 34: 52,
 95: 53, 96: 54, 22: 55, 23: 56, 42: 57, 10: 58, 54: 59, 55: 60, 56: 61, 57: 62,
 53: 63, 114: 64, 115: 65, 116: 66, 117: 67, 118: 68, 119: 69, 120: 70, 121: 71,
 122: 72, 123: 73, 124: 74, 125: 75, 126: 76, 59: 78, 60: 79, 61: 80, 62: 81,
 128: 82, 16: 83, 130: 84, 131: 85, 132: 86, 133: 87, 134: 88, 135: 89, 136: 90,
 137: 91,
 # mt2009 points with no APPLY id at all (length.h 138..168 - the engine
 # applies them straight from the item). A pseudo key of 1000 + point, so
 # the label tables can name them; without it the panel wrote "Bonus #139".
 138: 1138, 139: 1139, 140: 1140, 141: 1141, 142: 1142, 143: 1143, 144: 1144, 145: 1145, 146: 1146, 147: 1147, 148: 1148, 149: 1149, 150: 1150, 151: 1151, 152: 1152, 153: 1153, 154: 1154, 155: 1155, 156: 1156, 157: 1157, 158: 1158, 159: 1159, 160: 1160, 161: 1161, 162: 1162, 163: 1163, 164: 1164, 165: 1165, 166: 1166, 167: 1167, 168: 1168}
# The kingdom of a character: the index, then (r40250 only) the account.
EMPIRE_EXPR = "COALESCE(NULLIF(pi.empire,0),0)" if ENGINE_MT2009 else "COALESCE(NULLIF(pi.empire,0),a.empire,0)"

JOB_NAMES = ("Wojownik", "Ninja", "Sura", "Szaman")
SKILLS = {
    # Exact vnum/name pairs from Tieru's current panel. The old mapping put
    # display names next to the wrong VNUMs, hence correct icons looked wrong.
    (0, 1): ((1, "Trzystronne Cięcie"), (2, "Wir Miecza"), (3, "Berserk"), (4, "Aura Miecza"), (5, "Szarża")),
    (0, 2): ((16, "Duchowe Uderzenie"), (17, "Tąpnięcie"), (18, "Uderzenie Miecza"), (19, "Silne Ciało"), (20, "Walnięcie")),
    (1, 1): ((31, "Zasadzka"), (32, "Szybki Atak"), (33, "Wirujący Sztylet"), (34, "Krycie się"), (35, "Trująca Chmura")),
    (1, 2): ((46, "Powtarzalny Strzał"), (47, "Deszcz Strzał"), (48, "Ognista Strzała"), (49, "Bezszelestny Chód"), (50, "Trująca Strzała")),
    (2, 1): ((61, "Uderzenie Palcem"), (62, "Smoczy Wir"), (63, "Czarowane Ostrze"), (64, "Strach"), (65, "Czarowana Zbroja"), (66, "Rozproszenie Magii")),
    (2, 2): ((76, "Mroczne Uderzenie"), (77, "Ogniste Uderzenie"), (78, "Ognisty Duch"), (79, "Mroczna Ochrona"), (80, "Duchowy Cios"), (81, "Mroczna Sfera")),
    (3, 1): ((91, "Latający Talizman"), (92, "Strzelający Smok"), (93, "Smoczy Skowyt"), (94, "Błogosławieństwo"), (95, "Odbicie"), (96, "Pomoc Smoka")),
    (3, 2): ((106, "Błyskawiczny Rzut"), (107, "Przywołanie Błyskawicy"), (108, "Burzowy Szpon"), (109, "Leczenie"), (110, "Zwinność"), (111, "Zwiększenie Ataku")),
}
try:
    ITEM_ICONS = json.loads((Path(__file__).parent / "static" / "item_icons.json").read_text(encoding="utf-8"))
except (OSError, json.JSONDecodeError):
    ITEM_ICONS = {}
try:
    EXP_LEVELS = json.loads((Path(__file__).parent / "static" / "exp_levels.json").read_text(encoding="utf-8"))
except (OSError, json.JSONDecodeError):
    EXP_LEVELS = [0]
try:
    GM_COMMANDS = (Path(__file__).parent / "gm_commands.txt").read_text(encoding="utf-8", errors="replace")
except OSError:
    GM_COMMANDS = "Brak pliku z komendami."


# MyISAM nie przezywa nieczystego zatrzymania, a ten panel czyta na stronie
# glownej najruchliwsza tabele w calym swiecie - log.log, dla rankingu wedkarzy.
# Gdy jest uszkodzona, kazde zapytanie do niej rzuca wyjatkiem, Flask pokazuje
# wlasne "Internal Server Error", i to zrzut ekranu tej strony trafia na
# Discorda - bez nazwy tabeli, bez przyczyny, bez niczego do zrobienia
# (archonek, 10 wrzesnia: "klikam i blad wyskakuje"; zwykly panel dzialal, bo
# jego strona glowna do log.log nie zaglada). Aktualizacja tego nie naprawia:
# uszkodzenie siedzi w danych na wolumenie, nie w obrazie.
#
# Numery bledow: 1194 "is marked as crashed and should be repaired",
# 1195 i 144 "last repair failed", 145 to samo dla starszych serwerow.
CRASHED_TABLE_ERRNOS = (144, 145, 1194, 1195)


@app.errorhandler(pymysql.err.OperationalError)
def handle_crashed_table(error):
    errno = error.args[0] if error.args else 0
    message = str(error.args[1]) if len(error.args) > 1 else str(error)
    if errno not in CRASHED_TABLE_ERRNOS:
        # Nie nasza sprawa - niech Flask pokaze swoje 500 i zapisze slad.
        raise error
    table = ""
    match = re.search(r"Table '([^']+)'", message)
    if match:
        table = match.group(1).replace("./", "").replace("/", ".")
    named = ("Tabela <code>%s</code>" % escape(table)) if table else "Jedna z tabel bazy"
    body = """<!doctype html><html lang="pl"><head><meta charset="utf-8">
<title>Uszkodzona tabela bazy</title>
<style>body{font-family:system-ui,Segoe UI,Arial,sans-serif;max-width:52em;margin:3em auto;padding:0 1.5em;line-height:1.6;color:#222}
h1{font-size:1.5em}code{background:#f2f2f2;padding:.15em .35em;border-radius:3px}
pre{background:#f2f2f2;padding:1em;border-radius:5px;overflow-x:auto}
.note{background:#fff8e1;border-left:4px solid #e0a800;padding:.8em 1em;margin:1.5em 0}</style>
</head><body>
<h1>Uszkodzona tabela bazy danych</h1>
<p>%s jest oznaczona jako uszkodzona, wiec panel nie moze jej odczytac.
Silnik gry uzywa tabel MyISAM, a te nie przezywaja nagłego zatrzymania -
wystarczy zamkniecie Dockera w trakcie zapisu albo zanik zasilania.</p>
<div class="note"><strong>Aktualizacja serwera tego nie naprawi.</strong>
Uszkodzenie jest w danych na dysku, a nie w programie - nowa wersja czyta te
same pliki.</div>
<h2>Jak naprawic</h2>
<p>Otworz PowerShell w folderze serwera, w podkatalogu <code>linux-port\\docker</code>
(w launcherze przycisk FOLDER SERWERA), i uruchom:</p>
<pre>docker compose exec mariadb mysqlcheck -uroot -p --auto-repair --databases log player account common</pre>
<p>Zapyta o haslo - to <code>M2_DB_ROOT_PASSWORD</code> z pliku <code>.env</code>
w tym samym folderze. Naprawa duzej tabeli logow potrafi potrwac kilka minut.</p>
<h2>Jesli naprawa sie nie uda</h2>
<p>Baza <code>log</code> to wylacznie historia: co kto podniosl, ulepszyl i
powiedzial. Gra jej nie czyta i zadna postac, przedmiot ani bot od niej nie
zaleza. Jesli <code>mysqlcheck</code> zglosi, ze nie da rady, mozna te tabele
oproznic bez straty dla swiata:</p>
<pre>docker compose exec mariadb mariadb -uroot -p -e "TRUNCATE log.log; TRUNCATE log.levellog; TRUNCATE log.shout_log;"</pre>
<div class="note">Nie rob tego dla baz <code>player</code>, <code>account</code>
ani <code>common</code> - tam sa postacie, konta i boty.</div>
<p style="margin-top:2em;color:#666;font-size:.9em">Blad bazy: %s (%s)</p>
</body></html>""" % (named, errno, escape(message))
    return body, 500



def db():
    return pymysql.connect(
        host=os.environ.get("DB_HOST", "mariadb"), port=int(os.environ.get("DB_PORT", "3306")),
        user=os.environ["DB_USER"], password=os.environ["DB_PASSWORD"],
        charset="utf8mb4", cursorclass=pymysql.cursors.DictCursor, autocommit=True,
    )


def rows(sql, params=()):
    with db() as con:
        with con.cursor() as cur:
            cur.execute(sql, params)
            return cur.fetchall()


def one(sql, params=()):
    result = rows(sql, params)
    return result[0] if result else {}


def _ensure_collector_tables():
    """Same schema collector.py's own init() creates, run here too at
    startup. Without this, a panel that comes up before the collector's
    first successful cycle (e.g. right after `docker compose up`, before
    MariaDB finishes its own startup) 500s on every web_seban_* table --
    and if that first collector attempt fails, it does not retry until its
    full interval (default 300s) has passed, not right away. Reported by
    players as the panel "Internal Server Error"-ing for the first ~5
    minutes after an update (sizowski, 2026-09-14)."""
    try:
        import collector
        with db() as con, con.cursor() as cur:
            collector.init(cur)
    except pymysql.MySQLError as exc:
        app.logger.warning("could not pre-create collector tables at startup: %s", exc)


_ensure_collector_tables()


def game_text(value):
    if isinstance(value, bytes):
        for encoding in ("cp1250", "utf-8", "latin1"):
            try:
                return value.decode(encoding)
            except UnicodeDecodeError:
                pass
        return value.decode("cp1250", "replace")
    return value or ""


def cp1250_hex_text(value):
    """Decode a Polish item name without trusting the log table's charset."""
    try:
        return bytes.fromhex(str(value or "")).decode("cp1250")
    except (TypeError, ValueError, UnicodeDecodeError):
        return ""


def map_name(index):
    """Name only maps which this Playerbots world actually runs."""
    index = int(index or 0)
    return MAP_NAMES.get(index, f"Poza aktywnym światem (mapa #{index})")


def changelog_entries():
    """Read version notes from the repository file for the public in-panel log."""
    path = Path(__file__).parent / "CHANGELOG.md"
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return []
    entries, current = [], None
    for line in lines:
        if line.startswith("## "):
            if current:
                entries.append(current)
            heading = line[3:].strip()
            timestamp, separator, version = heading.partition(" · ")
            current = {"timestamp": timestamp if separator else "Wcześniejsza wersja", "version": version if separator else heading, "changes": []}
        elif current and line.startswith("- "):
            current["changes"].append(line[2:].strip())
    if current:
        entries.append(current)
    return entries


def settings():
    values = dict(DEFAULT_SETTINGS)
    try:
        for row in rows("SELECT name,value FROM player.web_seban_settings"):
            if row["name"] in values:
                values[row["name"]] = str(row["value"])
    except pymysql.MySQLError:
        pass
    return values


def write_settings(values):
    """Persist panel-only configuration without relying on environment secrets."""
    with db() as con:
        with con.cursor() as cur:
            cur.execute("""CREATE TABLE IF NOT EXISTS player.web_seban_settings (
              name VARCHAR(64) NOT NULL PRIMARY KEY, value VARCHAR(255) NOT NULL,
              updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP) ENGINE=InnoDB""")
            cur.executemany(
                "INSERT INTO player.web_seban_settings (name,value) VALUES (%s,%s) ON DUPLICATE KEY UPDATE value=VALUES(value)",
                tuple(values.items()),
            )


def validate_display_settings(form):
    name = form.get("panel_name", "").strip()[:48]
    try:
        stuck = max(1, min(120, int(form.get("stuck_minutes", "5"))))
    except (TypeError, ValueError):
        stuck = 5
    theme, monitor_mode = form.get("theme", "ocean"), form.get("monitor_mode", "vps")
    if not name:
        return None, "Nazwa panelu nie może być pusta."
    if theme not in ("ocean", "ember", "forest") or monitor_mode not in ("vps", "docker"):
        return None, "Nieprawidłowe ustawienia wyglądu lub monitoringu."
    return {"panel_name": name, "stuck_minutes": str(stuck), "theme": theme, "monitor_mode": monitor_mode}, None


def skill_rank(master_type, level):
    master_type, level = int(master_type or 0), int(level or 0)
    if master_type >= 3 or level >= 40:
        return "P"
    if master_type == 2 or level >= 30:
        return f"G{max(1, level - 29)}"
    if master_type == 1 or level >= 20:
        return f"M{max(1, level - 19)}"
    return str(level)


def experience_progress(level, exp):
    level, exp = int(level or 0), max(0, int(exp or 0))
    required = int(EXP_LEVELS[min(max(level, 0), len(EXP_LEVELS) - 1)] or 0)
    return {"current": exp, "required": required, "percent": min(100, round(exp * 100 / required, 1)) if required else 100}


def honor_rank(value):
    """The core stores alignment in tenths; return the in-game value and colour."""
    points = int(float(value or 0) / 10)
    bands = (
        (12000, "Rycerski", "knightly"), (8000, "Szlachetny", "noble"),
        (4000, "Dobry", "good"), (1000, "Przyjazny", "friendly"),
        (0, "Neutralny", "neutral"), (-3999, "Agresywny", "aggressive"),
        (-7999, "Nieuczciwy", "dishonest"), (-11999, "Złośliwy", "malicious"),
        (-20000, "Okrutny", "cruel"),
    )
    for threshold, title, css in bands:
        if points >= threshold:
            return {"points": points, "title": title, "css": css}
    return {"points": points, "title": "Okrutny", "css": "cruel"}


# playerbot_status.tsv, read by its header: Iwakura's personalities (2.0.85)
# put four columns (persona, mood, mood_lock, lock_level) before the status
# text, which stays last because it may hold spaces. A core of before that
# writes the old fourteen columns under a header too; with no header at all
# the old fourteen are assumed.
STATUS_LEGACY_COLUMNS = ("pid", "personality", "ambition", "role", "in_party", "goal", "action",
                         "updated_ms", "map", "x", "y", "hp", "max_hp", "status")
PERSONA_NONE = 255


def parse_status_rows(text):
    header = None
    for line in text.splitlines():
        if line.startswith("pid\t"):
            header = line.split("\t")
            continue
        columns = header or STATUS_LEGACY_COLUMNS
        values = line.split("\t", len(columns) - 1)
        if len(values) != len(columns) or columns[-1] != "status":
            continue
        try:
            numbers = {name: int(value) for name, value in zip(columns[:-1], values[:-1])}
        except ValueError:
            continue
        if "pid" in numbers:
            yield numbers, values[-1]


def personality_label(state):
    """The personality that claims the bot now, or the old one with the
    PERSONA switch off."""
    persona = state.get("persona")
    if persona is not None:
        return BOT_PERSONAS.get(int(persona), f"#{persona}")
    return live_label("personality", state.get("personality"))


def mood_label(state):
    mood = state.get("mood")
    if mood is None:
        return ""
    text = BOT_MOODS.get(int(mood), "Normalny")
    lock = BOT_MOOD_LOCKS.get(int(state.get("mood_lock") or 0))
    return f"{text} ({lock})" if lock else text


def live_label(field, value):
    labels = {"personality": BOT_PERSONALITIES, "ambition": BOT_AMBITIONS, "goal": BOT_GOALS, "action": BOT_ACTIONS}.get(field, {})
    value = int(value or 0)
    return labels.get(value, f"#{value}")


def is_stationary_activity(status, action=None):
    try:
        if int(action or 0) in STATIONARY_ACTIONS:
            return True
    except (TypeError, ValueError):
        pass
    text = str(status or "").casefold()
    return any(marker in text for marker in ("łowi", "lowi", "ryb", "fishing", "czekam na branie"))


def apply_text(apply_type, value):
    key = int(apply_type or 0)
    if ENGINE_MT2009:
        key = POINT_TO_APPLY.get(key, key)
    name, suffix = APPLY_LABELS.get(key, (f"Bonus #{apply_type}", ""))
    value = int(value or 0)
    return f"{name} {value:+d}{suffix}"


def item_base_stats(vnum):
    """Client-side item properties displayed by the in-game tooltip.
    value1-4 alone are the item's +0 base -- refine level adds value5, once
    for a weapon's attack/magic-attack range and twice for Body/Shield (per
    Tieru's own tooltip JS, admin_panel.py 7788). Missing this made every
    refined weapon/armor show its +0 numbers: Różowa Szata+9 read 29
    defense here vs. 83 in the live client (29 + 27*2); Antyczny Dzwon+9
    read 50-70/35-60 here vs. 120-140/105-130 live (both +70). Reported by
    [GA]Seban, 2026-09-15."""
    proto = ITEM_DEFS.get(str(int(vnum or 0)), {})
    if not proto:
        return []
    stats, item_type, subtype = [], int(proto.get("type") or 0), int(proto.get("subtype") or 0)
    level = int(proto.get("level") or 0)
    if level:
        stats.append(f"Wymagany poziom: {level}")
    value = lambda index: int(proto.get(f"value{index}") or 0)
    refine_bonus = value(5)
    if item_type == 1:  # ITEM_WEAPON: magic 1/2, physical 3/4.
        attack_min, attack_max = value(3) + refine_bonus, value(4) + refine_bonus
        magic_min, magic_max = value(1) + refine_bonus, value(2) + refine_bonus
        if attack_min or attack_max:
            stats.append(f"Wartość ataku: {attack_min}–{attack_max}" if attack_min != attack_max else f"Wartość ataku: {attack_max}")
        if magic_min or magic_max:
            stats.append(f"Wartość magicznego ataku: {magic_min}–{magic_max}" if magic_min != magic_max else f"Wartość magicznego ataku: {magic_max}")
    elif item_type == 2:  # ITEM_ARMOR. Only these four subtypes show a flat
        # defense number in the client at all; jewelry (necklace/earring/
        # wrist) shows none, same as the real tooltip.
        multiplier = {0: 2, 1: 1, 2: 2, 4: 1}.get(subtype)
        if multiplier:
            defense = value(1) + refine_bonus * multiplier
            if defense:
                stats.append(f"Wartość obrony: {defense}")
    return stats


def empire_info(empire):
    try:
        return EMPIRES.get(int(empire), {"name": "—", "flag": ""})
    except (TypeError, ValueError):
        return {"name": "—", "flag": ""}


def empire_flag_path(empire):
    return empire_info(empire)["flag"]

def class_profile(job):
    try:
        return CLASS_PROFILES.get(int(job), CLASS_PROFILES[0])
    except (TypeError, ValueError):
        return CLASS_PROFILES[0]


def parse_skills(raw, job, group):
    if isinstance(raw, memoryview): raw = raw.tobytes()
    if isinstance(raw, str): raw = raw.encode("latin1", "ignore")
    raw = raw or b""
    result = []
    for vnum, name in SKILLS.get((int(job or 0) % 4, int(group or 0)), ()):
        offset = vnum * 6
        master, level = (raw[offset] if offset < len(raw) else 0), (raw[offset + 1] if offset + 1 < len(raw) else 0)
        rank = skill_rank(master, level)
        if level:
            # Tieru's icon pack has the master artwork in *_m.png.  It is used
            # for every mastered stage (M, G and P); there are no *_p.png files.
            result.append({"vnum": vnum, "name": name, "level": level, "master_type": master, "rank": rank, "icon_suffix": "_m" if master >= 1 or level >= 20 else ""})
    return result


# Passives with no client icon pack (confirmed against Tieru's own
# static/skill_icons/ -- none of these vnums are in it): horse riding/summon
# and the four ability-book skills (Dowodzenie..Polimorfia). Their "level"
# byte is a plain number here (30 lvl konia, 100% przywolania), not the
# M/G/P combat-skill grading skill_rank() computes for SKILLS above.
PASSIVE_SKILLS = {
    121: "Dowodzenie", 122: "Combo", 124: "Górnictwo", 125: "Kowalstwo",
    126: "Język Shinsoo", 127: "Język Chunjo", 128: "Język Jinno", 129: "Polimorfia",
    130: "Poziom konia", 131: "Przywołanie konia",
}


def parse_passive_skills(raw):
    if isinstance(raw, memoryview): raw = raw.tobytes()
    if isinstance(raw, str): raw = raw.encode("latin1", "ignore")
    raw = raw or b""
    result = []
    for vnum, name in PASSIVE_SKILLS.items():
        offset = vnum * 6
        level = raw[offset + 1] if offset + 1 < len(raw) else 0
        if level:
            result.append({"vnum": vnum, "name": name, "level": level})
    return result


SKILL_NAMES = {vnum: name for skill_set in SKILLS.values() for vnum, name in skill_set}
# Every ordinary Skill Book is vnum 50300 no matter which skill it teaches --
# the skill itself only lives in socket0 (the "Instr." vnums from 50401 up
# already carry their skill in locale_name and never need this). Kept as a
# set, not a bare constant, in case another generic-book vnum shows up later.
SKILLBOOK_VNUMS = {50300}


def resolve_item_display_name(vnum, socket0, base_name):
    """base_name with the real skill substituted in for a generic Skill Book.
    Takes vnum/socket0 as plain values, not an item row, so it can be called
    from a GROUP BY (vnum, socket0) aggregate later too (see economy()/
    economy_shops(), which today count every Skill Book as one vnum) -- not
    only from a single player's item row."""
    if int(vnum or 0) in SKILLBOOK_VNUMS:
        skill_name = SKILL_NAMES.get(int(socket0 or 0))
        if skill_name:
            return f"{skill_name} — Księga Umiejętności"
    return base_name


_season_cache = {"at": 0.0, "weekly": [], "records": {}}


def news_feed_events():
    """Curate rare achievements from the native game log with stable IDs."""
    # Filter in SQL before the limit.  A busy server produces thousands of
    # ordinary +0–+3 refines per minute; taking its newest 900 rows first made
    # rare achievements disappear from the feed altogether.
    raw = rows("""SELECT l.time,l.how,l.hint,HEX(l.hint) AS hint_hex,l.what,l.who,p.name,
        HEX(proto.locale_name) AS item_name_hex
      FROM log.log l JOIN player.player p ON p.id=l.who
      LEFT JOIN player.item i ON i.id=l.what
      LEFT JOIN player.item_proto proto ON proto.vnum=i.vnum
      WHERE l.time >= NOW() - INTERVAL 12 HOUR
        AND (
          (l.how='REFINE SUCCESS' AND (l.hint LIKE '%%+7%%' OR l.hint LIKE '%%+8%%' OR l.hint LIKE '%%+9%%'))
          OR l.how='SKILLUP'
          OR (l.how='GET' AND LOWER(CONVERT(l.hint USING utf8mb4)) COLLATE utf8mb4_general_ci LIKE '%%małż%%')
        )
      ORDER BY l.time DESC LIMIT 900""")
    events, seen = [], set()
    for row in raw:
        # `how` is VARBINARY on mt2009 and arrives as bytes; str() of that is
        # "b'GET'" and matches nothing below.
        how, name = game_text(row.get("how")), game_text(row.get("name"))
        # log.log's hint column is declared big5 while the engine writes CP1250
        # into it (see CLAUDE.md), so letting the driver decode the column gives
        # mojibake for anything past ASCII - "Skorzane" came back as
        # "SkAtrzane". HEX(l.hint) sidesteps whatever charset MySQL believes the
        # column has and returns the untouched bytes, which really are CP1250 -
        # the same trick this function already uses for item_proto.locale_name
        # below. Falls back to the driver's own decode if the hex round trip
        # fails. Patch by seban latino, 13 September.
        hint = cp1250_hex_text(row.get("hint_hex")) or game_text(row.get("hint"))
        key = f"{how}:{row.get('who')}:{row.get('what')}:{row.get('time')}"
        if key in seen or not name:
            continue
        message = None
        if how == "REFINE SUCCESS":
            match = re.search(r"\+([789])(?:\s|$)", hint)
            if match:
                item_name = cp1250_hex_text(row.get("item_name_hex")) or hint.strip()
                message = f"{name} ulepszył {item_name}"
        elif how == "SKILLUP":
            match = re.search(r"SkillUp:\s+\S+\s+(\d+)\s+(\d+)\s+(\d+)", hint)
            if match:
                vnum, master, level = map(int, match.groups())
                rank = skill_rank(master, level)
                if (rank.startswith("M") and rank != "M1") or rank.startswith("G") or rank == "P":
                    message = f"{name} rozwinął {SKILL_NAMES.get(vnum, f'umiejętność #{vnum}')} na {rank}"
        elif how == "GET" and "małż" in hint.casefold():
            message = f"{name} znalazł Małż podczas połowu"
        if message:
            seen.add(key)
            events.append({"key": key, "time": row["time"].strftime("%H:%M") if hasattr(row.get("time"), "strftime") else str(row.get("time"))[11:16], "message": message, "refine_tier": int(match.group(1)) if how == "REFINE SUCCESS" and match else 0})
    return list(reversed(events[-30:]))


def live_statuses():
    result = {}
    for pattern in STATUS_GLOBS:
        for path in Path("/").glob(pattern.lstrip("/")):
            try:
                if datetime.now().timestamp() - path.stat().st_mtime > 25:
                    continue
                # Kanal z nazwy katalogu (var/channelN/<rdzen>/): jeden plik to
                # jeden rdzen jednego kanalu, a bot jest naraz tylko na jednym.
                channel = 1
                for part in path.parts:
                    if part.startswith("channel") and part[7:].isdigit():
                        channel = int(part[7:])
                for n, status in parse_status_rows(path.read_text(encoding="cp1250", errors="replace")):
                    persona = n.get("persona", PERSONA_NONE)
                    mood = n.get("mood", PERSONA_NONE)
                    result[n["pid"]] = {"personality": n.get("personality", 0), "ambition": n.get("ambition", 0), "role": n.get("role", 0), "in_party": bool(n.get("in_party", 0)), "goal": n.get("goal", 0), "action": n.get("action", 0), "updated_ms": n.get("updated_ms", 0), "map_index": n.get("map", 0), "x": n.get("x", 0), "y": n.get("y", 0), "hp": n.get("hp", 0), "max_hp": n.get("max_hp", 0),
                                        "persona": None if persona == PERSONA_NONE else persona, "mood": None if mood == PERSONA_NONE else mood,
                                        "mood_lock": n.get("mood_lock", 0), "lock_level": n.get("lock_level", 0), "status": status, "channel": channel}
            except (OSError, ValueError):
                continue
    return result


def guild_statuses():
    """Scal raporty rdzeni Playerbots (odświeżane co minutę).
    Pola wspólne gildii bierzemy raz; botów online i exp sumujemy ze wszystkich
    rdzeni, bo każdy rdzeń widzi tylko własną część świata."""
    gathered, newest = {}, 0.0
    for pattern in GUILD_STATUS_GLOBS:
        for path in Path("/").glob(pattern.lstrip("/")):
            try:
                mtime = path.stat().st_mtime
                lines = path.read_text(encoding="cp1250", errors="replace").splitlines()
            except OSError:
                continue
            if not lines:
                continue
            newest = max(newest, mtime)
            columns = lines[0].rstrip("\r").split("\t")
            for line in lines[1:]:
                values = line.rstrip("\r").split("\t")
                if len(values) < len(columns):
                    continue
                row = dict(zip(columns, values))
                try:
                    gid, online = int(row.get("guild_id", 0)), int(row.get("online", 0))
                    strength, offered = int(row.get("avg_strength", 0)), int(row.get("exp_offered_here", 0))
                except ValueError:
                    continue
                if gid <= 0:
                    continue
                guild = gathered.get(gid)
                if guild is None:
                    guild = {"id": gid, "name": row.get("name", ""), "online": 0,
                             "strength_sum": 0, "exp_offered": 0, "master": row.get("master", ""),
                             "war_with": row.get("war_with", ""), "next_war_in_s": None}
                    for key in ("empire", "tier", "level", "members", "master_pid", "ladder", "wins", "draws", "losses", "war_score", "war_enemy_score"):
                        try: guild[key] = int(row.get(key, 0))
                        except ValueError: guild[key] = 0
                    gathered[gid] = guild
                guild["online"] += online
                guild["strength_sum"] += strength * online
                guild["exp_offered"] += offered
                if not guild["master"] and row.get("master"):
                    guild["master"] = row["master"]
                if not guild["war_with"] and row.get("war_with"):
                    guild["war_with"] = row["war_with"]
                try: next_war = int(row.get("next_war_in_s", -1))
                except ValueError: next_war = -1
                previous = guild["next_war_in_s"]
                if next_war >= 0 and (previous is None or previous < 0 or next_war < previous):
                    guild["next_war_in_s"] = next_war
    if not gathered or time.time() - newest > 300:
        return [], None
    result = []
    for guild in gathered.values():
        guild["avg_strength"] = guild["strength_sum"] // guild["online"] if guild["online"] else 0
        guild["tier_label"] = GUILD_TIERS.get(guild["tier"], "Zwykła")
        result.append(guild)
    result.sort(key=lambda g: (g["tier"], -g["level"], -g["members"], g["name"].casefold()))
    return result, newest


def guild_war_text(seconds):
    if seconds is None or seconds < 0:
        return "brak zaplanowanej"
    if seconds == 0:
        return "trwa teraz"
    minutes = max(1, (seconds + 59) // 60)
    return f"za ok. {minutes} min"


def live_map_counts():
    counts = {}
    for entry in live_statuses().values():
        index = entry["map_index"]
        counts[index] = counts.get(index, 0) + 1
    return [{"map_index": index, "character_count": count} for index, count in sorted(counts.items(), key=lambda item: -item[1])]


def live_bots():
    statuses = live_statuses()
    if not statuses:
        return []
    ids = list(statuses)
    placeholders = ",".join(["%s"] * len(ids))
    roster = rows(f"""
        SELECT p.id, p.name, p.level, p.job, p.horse_level FROM player.player p
        LEFT JOIN account.account a ON a.id=p.account_id
        WHERE p.id IN ({placeholders}) AND (LEFT(a.login,10)='playerbot_' OR p.name LIKE 'bot%%')
    """, ids)
    threshold = max(1, min(120, int(settings().get("stuck_minutes", "5"))))
    historical = {}
    try:
        prior = rows("""SELECT s.pid,s.map_index,s.x,s.y FROM player.web_seban_bot_position_snapshot s
          JOIN (SELECT pid, MAX(captured_at) captured_at FROM player.web_seban_bot_position_snapshot
                WHERE captured_at <= NOW() - INTERVAL %s MINUTE GROUP BY pid) old
          ON old.pid=s.pid AND old.captured_at=s.captured_at WHERE s.pid IN (""" + placeholders + ")", (threshold, *ids))
        historical = {row["pid"]: row for row in prior}
    except pymysql.MySQLError:
        pass
    result = []
    for bot in roster:
        state = statuses.get(bot["id"])
        if state and state["map_index"] in MAP_BOUNDS:
            old = historical.get(bot["id"])
            stuck = bool(old and old["map_index"] == state["map_index"] and (old["x"] - state["x"]) ** 2 + (old["y"] - state["y"]) ** 2 < 40000 and not is_stationary_activity(state.get("status"), state.get("action")))
            # The free-text status is diagnostic and can be stale; action is the authoritative core state.
            result.append({
                **bot, **state,
                "personality_label": personality_label(state),
                "mood_label": mood_label(state),
                "ambition_label": live_label("ambition", state["ambition"]),
                "goal_label": live_label("goal", state["goal"]),
                "action_label": live_label("action", state["action"]),
                "stuck": stuck,
                "fighting_metin": int(state.get("goal") or 0) == 7 and int(state.get("action") or 0) == 2,
            })
    return result



def fishing_diagnostics():
    bots = live_bots()
    anglers = [bot for bot in bots if int(bot.get("action") or 0) == 14]
    matches = []
    for path in Path("/opt/metin2/var").glob("channel*/*/syslog"):
        try:
            with path.open("rb") as handle:
                handle.seek(max(0, path.stat().st_size - 262144))
                text = handle.read().decode("latin-1", "ignore")
        except OSError:
            continue
        for line in text.splitlines():
            if re.search(r"fish|fishing|w[ęe]dk|rod", line, re.I):
                matches.append({"core": path.parent.name, "line": line[-300:]})
    database_events = 0
    try:
        found = one("SELECT COUNT(*) AS total FROM log.log WHERE how LIKE %s OR hint LIKE %s", ("%FISH%", "%FISH%"))
        database_events = int(found.get("total") or 0) if found else 0
    except pymysql.MySQLError:
        pass
    return {"weights": read_ai_weights(), "online": len(bots), "anglers": anglers,
            "log_matches": matches[-80:], "database_events": database_events}

def read_rates():
    """What the operator set, from the one place the engine reads it.

    This used to prefer the spool's rates.status, which is only this panel's
    echo of its own last request and m2-rates' echo of the one it carried out.
    The classic panel's in-game RATES helper and the timed events write the
    flags without touching that file, so it goes stale and this page then
    showed - and on the next Save re-imposed - numbers the world had left
    behind: measured on the test world on 20 September, the file said drop 150
    / yang 120 against 200 / 200 in the flags ("jak ustawialem wczesniej raty u
    tiera to u sebana narzucal poprzednie", NerrVoVy). On mt2009 the flags are
    the truth; rates.status stays the truth on r40250, which has no flags and
    whose m2-rates rewrites the tables itself.

    m2_event_*_base is what the operator set while an event boosts the live
    flag, so it wins where it is set - a Save during an event must not turn the
    boost into the new normal.
    """
    values = {name: 100 for name in RATE_NAMES}
    if ENGINE_MT2009:
        try:
            wanted = []
            for name, flags in MT2009_RATE_FLAGS.items():
                wanted.append(flags[0])
                wanted.append("m2_event_%s_base" % MT2009_RATE_EVENT_KIND[name])
            live = {}
            for row in rows("SELECT szName, lValue FROM player.quest WHERE dwPID=0 AND szName IN (%s)"
                            % ",".join(["%s"] * len(wanted)), tuple(wanted)):
                live[row["szName"]] = int(row["lValue"])
            found = False
            for name, flags in MT2009_RATE_FLAGS.items():
                base = live.get("m2_event_%s_base" % MT2009_RATE_EVENT_KIND[name], 0)
                current = live.get(flags[0], 0)
                if base > 0:
                    values[name] = base
                    found = True
                elif current > 0:
                    values[name] = current
                    found = True
            if found:
                return values
        except (KeyError, TypeError, ValueError, pymysql.MySQLError):
            values = {name: 100 for name in RATE_NAMES}
    status = read_rate_status()
    if all(str(status.get(name, "")).isdigit() for name in RATE_NAMES):
        return {name: int(status[name]) for name in RATE_NAMES}
    try:
        for row in rows("SELECT name, value FROM player.web_admin_rates"):
            if row["name"] in values:
                values[row["name"]] = int(row["value"])
    except (KeyError, ValueError, pymysql.MySQLError):
        pass
    return values


def read_rate_status():
    result = {}
    try:
        for line in (RATES_SPOOL / "rates.status").read_text(encoding="utf-8", errors="replace").splitlines():
            key, separator, value = line.partition("=")
            if separator:
                result[key.strip()] = value.strip()
    except OSError:
        pass
    return result


def read_map_regen_status():
    status_file = RATES_SPOOL / "map-regens.status"
    result = {"state": "idle", "message": "Brak zapisanej zmiany", "values": {}, "stones": {}}
    try:
        for line in status_file.read_text(encoding="utf-8", errors="replace").splitlines():
            key, separator, value = line.partition("=")
            if not separator:
                continue
            if key.startswith("map_stone_") and key[10:].isdigit():
                result["stones"][int(key[10:])] = value
            elif key.startswith("map_") and key[4:].isdigit():
                result["values"][int(key[4:])] = value
            else:
                result[key] = value
    except OSError:
        pass
    return result


def read_spool_values(path):
    """Read a small key=value status file written by a fixed helper."""
    result = {}
    try:
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            key, separator, value = line.partition("=")
            if separator:
                result[key.strip()] = value.strip()
    except OSError:
        pass
    return result


def update_status():
    """State exposed by Tieru's isolated updater through its tiny spool."""
    result = read_spool_values(UPDATE_SPOOL / "update.status")
    try:
        age = max(0, int(time.time() - (UPDATE_SPOOL / "watcher").stat().st_mtime))
    except OSError:
        age = None
    result["watcher_age"] = age
    result["watcher_ready"] = age is not None and age < UPDATE_WATCHER_MAX_AGE_SECONDS
    result["state"] = result.get("state", "idle")
    try:
        step, steps = int(result.get("step", 0)), int(result.get("steps", 5))
        result["percent"] = max(0, min(100, round(step * 100 / max(steps, 1))))
    except ValueError:
        result["percent"] = 0
    if result["state"] == "running":
        result["percent"] = max(5, result["percent"])
    result["message"] = result.get("message", "Aktualizator czeka na zlecenie." if result["watcher_ready"] else "Aktualizator nie jest uruchomiony.")
    try:
        result["log"] = (UPDATE_SPOOL / "update.log").read_text(encoding="utf-8", errors="replace").splitlines()[-18:]
    except OSError:
        result["log"] = []
    return result


def installed_playerbots_version():
    """Read the live MT2009 version reported by the isolated updater watcher."""
    current = update_status()
    reported = str(current.get("version") or "").strip()
    if version_key(reported):
        return reported
    if current.get("state") == "ok":
        match = re.search(r"version ([0-9]+(?:\.[0-9]+)+)", current.get("message", ""))
        if match:
            return match.group(1)
    return os.environ.get("PLAYERBOTS_VERSION", "nieustawiona")

def version_key(value):
    match = re.fullmatch(r"v?([0-9]+(?:\.[0-9]+)+)", str(value or "").strip(), re.I)
    return tuple(int(part) for part in match.group(1).split(".")) if match else None


def latest_playerbots_release():
    """Read GitHub's latest release, cached so page loads never hammer the API."""
    now = time.time()
    if now - _playerbots_release_cache["checked_at"] < PLAYERBOTS_RELEASE_CACHE_SECONDS:
        return dict(_playerbots_release_cache)
    result = {"checked_at": now, "latest": None, "error": None}
    try:
        request_github = Request(PLAYERBOTS_RELEASE_URL, headers={"Accept": "application/vnd.github+json", "User-Agent": "Metin2-Singleplayer-Panel"})
        with urlopen(request_github, timeout=3) as response:
            payload = json.load(response)
        tag = str(payload.get("tag_name") or "").strip()
        if not version_key(tag):
            raise ValueError("GitHub nie zwrócił poprawnego numeru wydania.")
        result["latest"] = tag.lstrip("vV")
    except (OSError, ValueError, HTTPError, URLError, json.JSONDecodeError) as exc:
        result["error"] = str(exc)[:120] or "Nie udało się połączyć z GitHub."
    _playerbots_release_cache.clear()
    _playerbots_release_cache.update(result)
    return dict(result)


def playerbots_release_status():
    installed = installed_playerbots_version().strip()
    latest_info = latest_playerbots_release()
    latest = latest_info.get("latest")
    installed_key, latest_key = version_key(installed), version_key(latest)
    if installed_key and latest_key:
        behind = installed_key < latest_key
        if not behind:
            tone = "current"
        else:
            installed_parts = (installed_key + (0, 0, 0))[:3]
            latest_parts = (latest_key + (0, 0, 0))[:3]
            same_release_line = installed_parts[:2] == latest_parts[:2]
            patch_gap = latest_parts[2] - installed_parts[2]
            tone = "warning" if same_release_line and 1 <= patch_gap <= 3 else "outdated"
        return {"installed": installed, "latest": latest, "behind": behind,
                "tone": tone, "label": f"Dostępna {latest}" if behind else "Aktualna"}
    return {"installed": installed, "latest": latest, "behind": False, "tone": "unknown",
            "label": "Nie sprawdzono GitHub" if latest_info.get("error") else "Brak wersji lokalnej"}


def update_csrf_token():
    token = session.get("seban_update_csrf")
    if not token:
        token = uuid.uuid4().hex
        session["seban_update_csrf"] = token
    return token


def queue_tieru_update(update_seban_panel=False):
    """Request only the updater's fixed sequence; no command, URL or path crosses this boundary."""
    current = update_status()
    if not current["watcher_ready"]:
        raise RuntimeError("Aktualizator nie jest gotowy. Administrator musi uruchomić usługę updater.")
    if current.get("state") == "running":
        raise RuntimeError("Aktualizacja już trwa. Poczekaj na jej zakończenie.")
    version = installed_playerbots_version().strip()
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+)*", version):
        version = "0"
    request_id = "seban-" + uuid.uuid4().hex
    UPDATE_SPOOL.mkdir(parents=True, exist_ok=True)
    temporary = UPDATE_SPOOL / (request_id + ".new")
    try:
        update_panel = "1" if update_seban_panel else "0"
        temporary.write_text(
            f"id={request_id}\nversion={version}\ntime={int(time.time())}\nupdate_seban_panel={update_panel}\n",
            encoding="utf-8",
        )
        temporary.chmod(0o660)
        # replace is atomic. The worker records the id before doing work, so a
        # completed request never runs twice after a container recreation.
        os.replace(temporary, UPDATE_SPOOL / "request")
    finally:
        temporary.unlink(missing_ok=True)



EVENTS_FILE = RATES_SPOOL / "playerbot_events.tsv"
EVENT_KINDS = ("chest", "exp", "drop", "yang")
EVENT_LABELS = {
    "chest": "Szkatułki Blasku Księżyca",
    "exp": "Doświadczenie",
    "drop": "Drop przedmiotów",
    "yang": "Yang",
}
EVENT_DAY_NAMES = ("Pn", "Wt", "Śr", "Cz", "Pt", "Sb", "Nd")
EVENT_NOW_MINUTES = (15, 30, 60, 120, 180, 360)
EVENT_HHMM = re.compile(r"^([01]?\d|2[0-4]):([0-5]\d)$")


def event_hhmm(text):
    match = EVENT_HHMM.match((text or "").strip())
    if not match:
        return None
    hour, minute = int(match.group(1)), int(match.group(2))
    if hour == 24 and minute != 0:
        return None
    return f"{hour:02d}:{minute:02d}"


def read_events():
    rows, nows = [], {}
    try:
        lines = EVENTS_FILE.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return rows, nows
    for line in lines:
        line = line.rstrip("\r")
        if not line.strip():
            continue
        enabled = True
        if line.startswith("#off\t"):
            enabled, line = False, line[5:]
        elif line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) >= 4 and fields[0] == "now" and fields[1] in EVENT_KINDS:
            try:
                nows[fields[1]] = {"until": int(fields[2]), "value": int(fields[3])}
            except ValueError:
                pass
            continue
        if len(fields) < 5 or fields[0] not in EVENT_KINDS:
            continue
        start, end = event_hhmm(fields[2]), event_hhmm(fields[3])
        if not start or not end:
            continue
        try:
            value = int(fields[4])
        except ValueError:
            value = 0
        days = list(range(1, 8)) if fields[1] == "*" else [day for day in range(1, 8) if str(day) in fields[1].split(",")]
        rows.append({"kind": fields[0], "days": days, "start": start, "end": end,
                     "value": value, "on": enabled})
    return rows, nows


def write_events(rows, nows):
    RATES_SPOOL.mkdir(parents=True, exist_ok=True)
    body = [
        "# Metin2 Playerbots -- timed events, written by Seban Panel.",
        "# kind<TAB>days<TAB>from<TAB>to<TAB>value | now<TAB>kind<TAB>until_epoch<TAB>value",
        "# days: * or 1..7 (1 = Monday); #off keeps a disabled plan row.",
        "",
    ]
    for row in rows:
        days = "*" if len(row["days"]) == 7 else (",".join(str(day) for day in row["days"]) or "-")
        line = "%s\t%s\t%s\t%s\t%d" % (row["kind"], days, row["start"], row["end"], int(row["value"]))
        body.append(line if row.get("on", True) else "#off\t" + line)
    for kind in EVENT_KINDS:
        now_event = nows.get(kind)
        if now_event and int(now_event.get("until", 0)) > time.time():
            body.append("now\t%s\t%d\t%d" % (kind, int(now_event["until"]), int(now_event.get("value", 0))))
    temporary = EVENTS_FILE.with_suffix(".tsv.new")
    temporary.write_text("\n".join(body) + "\n", encoding="utf-8")
    os.replace(temporary, EVENTS_FILE)


def read_events_status():
    newest, newest_written = {}, 0
    for path in Path("/opt/metin2/var/channel1").glob("*/playerbot_events_status.tsv"):
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        current, written = {}, 0
        for line in lines[1:]:
            fields = line.rstrip("\r").split("\t")
            if len(fields) < 8 or fields[0] not in EVENT_KINDS:
                continue
            try:
                current[fields[0]] = {"scheduled": fields[1] == "1", "active": fields[2] == "1", "value": int(fields[3]), "until": int(fields[4]), "next_start": int(fields[5]), "next_value": int(fields[6])}
                written = int(fields[7])
            except ValueError:
                continue
        if current and written > newest_written:
            newest, newest_written = current, written
    if not newest or time.time() - newest_written > 300:
        return {}
    for item in newest.values():
        for key in ("until", "next_start"):
            stamp = item.get(key, 0)
            if stamp:
                item[key + "_text"] = time.strftime("%H:%M" if time.localtime(stamp).tm_yday == time.localtime().tm_yday else "%d.%m %H:%M", time.localtime(stamp))
    return newest

def read_ai_weights():
    """Read Tieru's live Playerbots goal weights; absent entries are neutral."""
    values = {key: AI_WEIGHT_NEUTRAL for key, _, _ in AI_WEIGHT_KEYS}
    values.update(AI_LIVE_DEFAULTS)
    try:
        for line in AI_WEIGHTS_FILE.read_text(encoding="utf-8", errors="replace").splitlines():
            fields = line.split("#", 1)[0].split()
            if len(fields) >= 2 and fields[0].upper() in values:
                try:
                    key, raw_value = fields[0].upper(), fields[1]
                    if key in ("CHAT", "BOOKS", "NIGHT", "LIFE", "WARS", "ISHOP"):
                        values[key] = 0 if raw_value.lower() in ("0", "off", "no") else 1
                    elif key in ("SCRAP", "REST"):
                        values[key] = max(0, min(100, int(raw_value)))
                    elif key in ("CHEST", "CHEST_STONE"):
                        values[key] = max(0, min(1000, int(raw_value)))
                    else:
                        values[key] = max(AI_WEIGHT_MIN, min(AI_WEIGHT_MAX, int(raw_value)))
                except ValueError:
                    pass
    except OSError:
        pass
    return values


def preserved_ai_weight_lines():
    """Keep new core keys this panel does not know about yet on every save."""
    known = {key for key, _, _ in AI_WEIGHT_KEYS} | AI_SPECIAL_WEIGHT_KEYS
    preserved = []
    try:
        for line in AI_WEIGHTS_FILE.read_text(encoding="utf-8", errors="replace").splitlines():
            fields = line.split("#", 1)[0].split()
            if len(fields) != 2:
                continue
            key, value = fields[0].upper(), fields[1]
            if key not in known and re.fullmatch(r"[A-Z][A-Z0-9_]{0,63}", key) and re.fullmatch(r"-?\d{1,10}", value):
                preserved.append(f"{key}\t{value}")
    except OSError:
        pass
    return preserved


def write_ai_weights(values):
    """Atomically replace known values without erasing newer-core settings."""
    RATES_SPOOL.mkdir(parents=True, exist_ok=True)
    content = [
        "# Metin2 Playerbots — wagi celów ustawione przez Seban Panel.",
        "# 25 = rzadko · 100 = domyślnie · 250 = często.",
        "# Rdzeń odczytuje plik co pięć sekund; restart nie jest wymagany.", "",
    ]
    content.extend(f"{key}\t{values[key]}" for key, _, _ in AI_WEIGHT_KEYS)
    content.append(f"CHAT\t{1 if values.get('CHAT', 1) else 0}")
    content.append(f"BOOKS\t{1 if values.get('BOOKS', 1) else 0}")
    content.append(f"NIGHT\t{1 if values.get('NIGHT', 1) else 0}")
    content.append(f"LIFE\t{1 if values.get('LIFE', 0) else 0}")
    content.append(f"WARS\t{1 if values.get('WARS', 1) else 0}")
    content.append(f"ISHOP\t{1 if values.get('ISHOP', 1) else 0}")
    content.append(f"SCRAP\t{max(0, min(100, int(values.get('SCRAP', 0))))}")
    content.append(f"REST\t{max(0, min(100, int(values.get('REST', 100))))}")
    for key in ("CHEST", "CHEST_STONE"):
        if values.get(key) is not None:
            content.append(f"{key}\t{max(0, min(1000, int(values[key])))}")
    content.extend(preserved_ai_weight_lines())
    temporary = AI_WEIGHTS_FILE.with_suffix(".tsv.new")
    temporary.write_text("\n".join(content) + "\n", encoding="utf-8")
    os.replace(temporary, AI_WEIGHTS_FILE)


def port_open(port):
    try:
        with socket.create_connection((GAME_HOST, port), timeout=0.4):
            return True
    except OSError:
        return False


def restart_progress():
    result = {}
    try:
        for line in (RATES_SPOOL / "server-settings.status").read_text(encoding="utf-8").splitlines():
            key, sep, value = line.partition("=")
            if sep:
                result[key] = value
        result["percent"] = max(0, min(100, int(result.get("percent", 0))))
        result["stage"] = result.get("message", "Oczekiwanie na stan serwera")
        if (RATES_SPOOL / "server-settings.request").exists() and result.get("state") != "running":
            result.update(state="running", percent=5, stage="Zlecenie oczekuje na serwer")
        return result
    except (OSError, ValueError):
        pass
    status = read_rate_status()
    auth, world = port_open(GAME_LOGIN_PORT), port_open(GAME_WORLD_PORT)
    state = status.get("state", "unknown")
    if state == "running":
        if not auth:
            return {"percent": 25, "stage": "Zatrzymywanie procesów gry", "state": state}
        if not world:
            return {"percent": 65, "stage": "Serwer logowania działa — uruchamianie świata", "state": state}
        return {"percent": 85, "stage": "Sprawdzanie kanału i usług", "state": state}
    if state == "ok" and auth and world:
        return {"percent": 100, "stage": "Serwer działa", "state": state}
    if state == "failed":
        return {"percent": 100, "stage": status.get("message", "Restart nie powiódł się"), "state": state}
    return {"percent": 100 if auth and world else 40, "stage": "Serwer działa" if auth and world else "Oczekiwanie na usługi", "state": state}


# On the mt2009 line a rate is not a rewritten table but six event flags the
# engine multiplies by (mob_exp / mob_item / mob_gold and their "_buyer"
# twins for premium accounts): rows of player.quest with dwPID = 0, read by the
# db core at boot and pushed to every game core. The game container has no
# database client, so the panel writes the rows and the restart it queues
# below is what makes the cores read them. See files/admin_panel.py, which
# also tries the in-game helper first; this console is a restart console.
MT2009_RATE_FLAGS = {
    "exp":  ("mob_exp",  "mob_exp_buyer"),
    "drop": ("mob_item", "mob_item_buyer"),
    "yang": ("mob_gold", "mob_gold_buyer"),
}
# The kind's name in the events' own base flags (playerbot_events.h), which
# hold what the operator set while an event boosts the live one.
MT2009_RATE_EVENT_KIND = {"exp": "exp", "drop": "drop", "yang": "yang"}


def persist_rates_mt2009(values):
    with db() as connection, connection.cursor() as cursor:
        for name, flags in MT2009_RATE_FLAGS.items():
            for flag in flags:
                cursor.execute("REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES (0, %s, '', %s)",
                               (flag, int(values[name])))
            # While a timed event runs, its base flag is the operator's
            # setting and the core keeps the live flag at the boost of it
            # (playerbot_events.h): a new setting goes to the base as well, or
            # the event put the old boost back and its end the old number.
            kind = MT2009_RATE_EVENT_KIND[name]
            for base_flag in ("m2_event_%s_base" % kind, "m2_event_%s_base_buyer" % kind):
                cursor.execute("UPDATE player.quest SET lValue=%s WHERE dwPID=0 AND szName=%s AND lValue>0",
                               (int(values[name]), base_flag))
        # The classic panel's table too, so both pages show the same numbers.
        try:
            for name in RATE_NAMES:
                cursor.execute("INSERT INTO player.web_admin_rates (name, value) VALUES (%s, %s) "
                               "ON DUPLICATE KEY UPDATE value=VALUES(value)", (name, int(values[name])))
        except pymysql.MySQLError:
            pass
        connection.commit()


def read_spawn_plan():
    values = read_spool_values(UPDATE_SPOOL / "spawn-plan.status")
    def number(key, default):
        try:
            return int(values.get(key, default))
        except (TypeError, ValueError):
            return default
    return {"window": max(1, min(180, number("window", 1))), "late_joiners": max(0, min(2500, number("late_joiners", 0))), "late_hours": max(1, min(168, number("late_hours", 24))), "state": values.get("state", "gotowy"), "message": values.get("message", "")}


def queue_spawn_plan(window, late_joiners, late_hours):
    stamp = int(time.time() * 1000)
    UPDATE_SPOOL.mkdir(parents=True, exist_ok=True)
    body = f"id=seban-spawn-{stamp}\nwindow={window}\nlate_joiners={late_joiners}\nlate_hours={late_hours}\ntime={int(time.time())}\n"
    temporary = UPDATE_SPOOL / "spawn-plan.request.new"
    temporary.write_text(body, encoding="utf-8")
    os.replace(temporary, UPDATE_SPOOL / "spawn-plan.request")
    (UPDATE_SPOOL / "spawn-plan.status").write_text(f"state=oczekuje\ntime={int(time.time())}\nwindow={window}\nlate_joiners={late_joiners}\nlate_hours={late_hours}\nmessage=Plan wejścia zapisany; oczekiwanie na restart gry.\n", encoding="utf-8")


def read_bot_count():
    """The bot-count target the game side will use on its next start.

    Mirrors read_rates(): m2-botcount publishes botcount.status on every
    boot and after every change, so this is the truth even across a
    container recreate the panel never saw happen. Falls back to counting
    who is actually alive right now (never zero on a running world) only
    when the spool has nothing at all, i.e. an image built before this
    existed.
    """
    status = read_spool_values(RATES_SPOOL / "botcount.status")
    value = status.get("count", "")
    if value.isdigit():
        return int(value)
    return len(live_bots()) or 350


def queue_botcount_change(count):
    """Ask the game side for a new playerbot target.

    Separate spool file from the rates on purpose: PLAYERBOT_AUTOSPAWN_COUNT
    is an environment variable a running container cannot be handed a new
    value for, so m2-botcount keeps the wanted number on the state volume
    and m2-supervise's start_core exports it fresh before every core exec
    (see m2-botcount's own header for the whole shape of it). Submitting
    this alongside a rates/respawn change can cost two restarts back to
    back instead of one combined one -- both are polled independently, a
    few seconds apart at most, which is a fair trade against merging two
    unrelated request formats into one.
    """
    stamp = int(time.time() * 1000)
    request_data = "\n".join((f"id=seban-{stamp}", f"count={count}", f"time={int(time.time())}", ""))
    RATES_SPOOL.mkdir(parents=True, exist_ok=True)
    temporary = RATES_SPOOL / "botcount.request.new"
    temporary.write_text(request_data, encoding="utf-8")
    os.replace(temporary, RATES_SPOOL / "botcount.request")
    (RATES_SPOOL / "botcount.status").write_text(
        "state=running\ntime=%s\ncount=%s\nmessage=restart requested by Seban Panel\n" %
        (int(time.time()), count), encoding="utf-8")


def read_student_chest_disabled():
    """Whether a new character (bot or player), of any class, is denied its
    starter chest (50187 warrior/sura, 50212 assassin, 50213 shaman).

    common.m2_switches is the same durable row apply.sh writes from
    M2_PLAYERBOT_DISABLE_STUDENT_CHEST at every playerbot-migrate start, and
    that starter_chest.quest reads live on a real player's first login --
    see that quest's own header for why a live read beats a cached one here.
    No row yet (a fresh install, or an image predating this switch) reads as
    "not disabled", matching the chest's original always-on behaviour.
    """
    try:
        row = one("SELECT value FROM common.m2_switches WHERE name='disable_student_chest'")
    except pymysql.MySQLError:
        return False
    return str(row.get("value", "0")) == "1"


def write_student_chest_disabled(disabled):
    """Flip the switch immediately, for real players' next login.

    This only ever touches the DB row a running quest reads live, so it
    needs no restart of anything -- unlike the rest of this page. It is
    still only half the story: a bot's own copy comes from a session
    variable apply.sh sets once at container start from
    M2_PLAYERBOT_DISABLE_STUDENT_CHEST, so this panel toggle covers real
    players' characters right away but leaves already-seeded bots and the
    .env default untouched, and a future playerbot-migrate run (a deploy, a
    host reboot) will reset this row back to whatever .env still says. Keep
    both in sync there if the choice should survive that.
    """
    rows(
        "INSERT INTO common.m2_switches (name, value) VALUES ('disable_student_chest', %s) "
        "ON DUPLICATE KEY UPDATE value = VALUES(value)",
        ("1" if disabled else "0",),
    )


def queue_rate_restart(values):
    if ENGINE_MT2009:
        persist_rates_mt2009(values)
    stamp = int(time.time() * 1000)
    request_data = "\n".join((
        f"id=seban-{stamp}",
        f"exp={values['exp']}",
        f"drop={values['drop']}",
        f"yang={values['yang']}",
        f"time={int(time.time())}",
        "",
    ))
    RATES_SPOOL.mkdir(parents=True, exist_ok=True)
    temporary = RATES_SPOOL / "request.new"
    temporary.write_text(request_data, encoding="utf-8")
    os.replace(temporary, RATES_SPOOL / "request")
    (RATES_SPOOL / "rates.status").write_text(
        "state=running\ntime=%s\nexp=%s\ndrop=%s\nyang=%s\nmessage=restart requested by Seban Panel\n" %
        (int(time.time()), values["exp"], values["drop"], values["yang"]), encoding="utf-8")


def server_settings_status():
    """Report whether the game-side restart helper is alive, not just queued."""
    ready = read_spool_values(RATES_SPOOL / "server-settings.ready")
    request = RATES_SPOOL / "server-settings.request"
    status = read_spool_values(RATES_SPOOL / "server-settings.status")
    try:
        ready_age = max(0, int(time.time() - (RATES_SPOOL / "server-settings.ready").stat().st_mtime))
    except OSError:
        ready_age = None
    try:
        request_age = max(0, int(time.time() - request.stat().st_mtime))
    except OSError:
        request_age = None
    worker_ready = ready.get("capability") == "server-settings" and ready_age is not None and ready_age <= SERVER_SETTINGS_READY_MAX_AGE_SECONDS
    result = {"ready": worker_ready, "ready_age": ready_age, "pending": request.exists(), "request_age": request_age,
              "can_clear": bool(request_age is not None and request_age >= SERVER_SETTINGS_STALE_SECONDS and not worker_ready),
              # mt2009 never needed the unified helper for any of this: rates,
              # bot count and map respawns are each their own small
              # restart-on-poll script (m2-rates / m2-botcount / m2-map-regens)
              # that m2-supervise already watches. queue_server_settings()
              # routes respawn changes through m2-map-regens directly on this
              # engine, so the banner below would be describing a gap that
              # does not exist here.
              "engine_handles_directly": ENGINE_MT2009 and CUSTOM_PATCHES_ENABLED}
    if worker_ready:
        result["message"] = "Helper ustawień serwera jest gotowy."
    elif result["pending"]:
        result["message"] = "Zlecenie nie jest odbierane przez helper gry. Sprawdź instalację integracji; po 10 minutach można usunąć wyłącznie zaległe zlecenie."
    elif ENGINE_MT2009 and CUSTOM_PATCHES_ENABLED:
        result["message"] = ("Ten silnik (mt2009) nie korzysta ze wspólnego helpera ustawień: "
                             "raty, docelowa liczba botów i respawny na mapach są obsługiwane "
                             "bezpośrednio przez m2-rates / m2-botcount / m2-map-regens w "
                             "kontenerze gry, każdy własnym restartem rdzeni.")
    else:
        # Telling the operator to install something this build never ships is
        # not help, and the warning fired on every visit to the console even
        # though both buttons that matter work without the helper.
        result["message"] = ("Ta wersja serwera nie zawiera silnikowej integracji Sebana, "
                             "więc zmiana respawnów map jest niedostępna. Restart serwera "
                             "i zmiana rat działają normalnie i niczego nie wymagają.")
    return result


RESTART_STALE_SECONDS = 600


def restart_in_flight():
    status = read_rate_status()
    if status.get("state") != "running":
        return False
    try:
        started = int(status.get("time", "0"))
    except (TypeError, ValueError):
        return False
    return 0 < time.time() - started < RESTART_STALE_SECONDS


def queue_server_settings(action, values=None, changes=None):
    """Publish a complete request only while the game-side helper is present."""
    support = server_settings_status()
    if not support["ready"]:
        # The settings helper (integration/m2-server-settings) is not part of
        # this image, so the map respawn half of the console has nothing to
        # carry it out and is refused with the message above. A plain restart,
        # and a rates-only apply, never needed it: the game container has
        # always watched the rates spool, and that is the path both buttons
        # took before 1.38 - refusing them here would put the dead restart
        # button of 1.30.19 back on every install without the helper.
        # An empty respawn field arrives as "reset", for every map, from a
        # form nobody touched - so "no respawn change" is "nothing but resets".
        respawn_changes = {key: value for key, value in (changes or {}).items() if value != "reset"}
        # This used to refuse the whole bundled save the moment ANY map field
        # held a non-empty, non-"reset" value -- including a leftover value
        # from earlier testing that the operator never touched this time
        # around. A rates/bot-count save has nothing to do with respawn and
        # must not be blocked by it; only the respawn half is unavailable
        # without the helper, so that half alone is skipped, with a
        # non-blocking flash instead of refusing the whole restart.
        if restart_in_flight():
            raise FileExistsError("a restart is already under way")
        queue_rate_restart(values if action == "apply" and values else read_rates())
        if changes and ENGINE_MT2009 and CUSTOM_PATCHES_ENABLED:
            # mt2009 needs no unified helper for this half either: m2-map-regens
            # (docker/game/bin) already rewrites every named map's regen.txt from
            # its own .m2orig snapshot and restarts the cores itself, the same
            # restart-on-poll spool as m2-rates and m2-botcount -- see that
            # script's own header for the whole shape of it. The full `changes`
            # dict is forwarded, resets included: a "reset" is the operator
            # asking for the map's shipped timing back, not a no-op to swallow.
            queue_map_regen_changes(changes)
        elif respawn_changes:
            flash("Zmiana respawnu na mapach wymaga integracji silnikowej Sebana, więc ją pominięto — "
                  "restart z pozostałymi ustawieniami został zlecony.", "warning")
        return
    RATES_SPOOL.mkdir(parents=True, exist_ok=True)
    request_id = "seban-" + uuid.uuid4().hex
    lines = [f"id={request_id}", f"action={action}", "source=panel"]
    if action == "apply":
        lines.extend(f"{key}={values[key]}" for key in RATE_NAMES)
        lines.extend(f"map_{key}={value}" for key, value in (changes or {}).items())
    temporary = RATES_SPOOL / (request_id + ".new")
    try:
        temporary.write_text("\n".join(lines) + "\n", encoding="utf-8")
        temporary.chmod(0o660)
        # A hard link is an atomic, exclusive publication on the shared volume.
        os.link(temporary, RATES_SPOOL / "server-settings.request")
    finally:
        temporary.unlink(missing_ok=True)


def queue_map_regen_changes(changes):
    stamp = int(time.time() * 1000)
    request_data = [f"id=seban-map-{stamp}", f"time={int(time.time())}"]
    request_data.extend(f"map_{map_index}={value}" for map_index, value in changes.items())
    RATES_SPOOL.mkdir(parents=True, exist_ok=True)
    temporary = RATES_SPOOL / "map-regens.request.new"
    temporary.write_text("\n".join(request_data) + "\n", encoding="utf-8")
    os.replace(temporary, RATES_SPOOL / "map-regens.request")
    (RATES_SPOOL / "map-regens.status").write_text(
        "state=running\ntime=%s\nmessage=Zapisano zestaw zmian respawnu; rdzenie zostaną ponownie uruchomione.\n" % int(time.time()),
        encoding="utf-8",
    )


def queue_map_regen_change(map_index, action, seconds=None):
    """Backward-compatible single-map queue entry."""
    queue_map_regen_changes({int(map_index): "reset" if action == "reset" else int(seconds)})


def biologist_missions():
    """Known Biologist missions plus names already emitted by the game."""
    names = set(BIOLOGIST_FALLBACK_MISSIONS)
    try:
        discovered = rows("""SELECT DISTINCT szName FROM player.quest
                           WHERE szName REGEXP '^(make_herb_lv[0-9]+|collect_quest_lv[0-9]+)$'""")
        names.update(row["szName"] for row in discovered if row.get("szName"))
    except pymysql.MySQLError:
        pass

    def mission_order(name):
        match = re.search(r"([0-9]+)$", name)
        return (int(match.group(1)) if match else 0, name)

    return tuple(sorted(names, key=mission_order))


# ---------------------------------------------------------------------------
# What makes a character a bot, in one place instead of eight.
#
# The name used to be the test: everything this project creates is called
# bot<something>, so `name LIKE 'bot%'` found them all. Rename them - which is
# exactly what the Discord keeps asking for, human nicknames instead of
# botarek7 - and every ranking, the live map, the world statistics and the
# season page quietly stop counting them.
#
# The core never asks the name. CPlayerBotManager::LoadRegisteredBots accepts a
# character only when its account login is exactly playerbot_NNN, and renaming a
# character does not touch an account login. So that is what is asked here too,
# with the old name test kept beside it, so a hand-made bot on an ordinary
# account stays visible exactly as before.
#
# The classic panel has had this since it was bitten by the same thing; this is
# the same predicate, spelled for the aliases these queries use.
def bot_identity(alias="p"):
    ref = (alias + ".") if alias else ""
    return ("(EXISTS (SELECT 1 FROM account.account ba"
            " WHERE ba.id = " + ref + "account_id"
            " AND LEFT(ba.login, 10) = 'playerbot_')"
            " OR " + ref + "name LIKE 'bot%%')")


BOT_IS = bot_identity("p")
BOT_IS_BARE = bot_identity("")


def include_real_players_in_rankings():
    """/manage toggle: rankingi/leaderboardy licza tylko playerboty domyslnie,
    albo kazda postac (w tym prawdziwych graczy) gdy operator to wlaczy --
    zgloszone przez gracza NerrVoVy na Discordzie, 2026-09-15, zeby granie
    obok botow bylo bardziej immersyjne."""
    # common.m2_switches is Seban's own table: the collector creates it at
    # start since 1.54.1+Playerbots 2.0.55, but a panel asked before that,
    # or on a database it cannot create in, reads "off" rather than 500 on
    # every ranking and the dashboard (Playerbots 2.0.55).
    try:
        row = one("SELECT value FROM common.m2_switches WHERE name='include_real_players_in_rankings'")
    except pymysql.MySQLError:
        return False
    return str(row.get("value", "0")) == "1"


def write_include_real_players_in_rankings(enabled):
    rows(
        "INSERT INTO common.m2_switches (name, value) VALUES ('include_real_players_in_rankings', %s) "
        "ON DUPLICATE KEY UPDATE value = VALUES(value)",
        ("1" if enabled else "0",),
    )


def read_announce_plus9_refines():
    """/manage toggle: server-wide gold announcement (same notice_all() /b
    uses) when a real player upgrades something to +9. Never for bots --
    they refine to +9 constantly, that would be pure spam. The collector
    polls for this switch and queues the actual notice_all() call through
    web_admin.quest; see collector.py's check_plus9_refines()."""
    try:
        row = one("SELECT value FROM common.m2_switches WHERE name='announce_plus9_refines'")
    except pymysql.MySQLError:
        return False
    return str(row.get("value", "0")) == "1"


def write_announce_plus9_refines(enabled):
    rows(
        "INSERT INTO common.m2_switches (name, value) VALUES ('announce_plus9_refines', %s) "
        "ON DUPLICATE KEY UPDATE value = VALUES(value)",
        ("1" if enabled else "0",),
    )


def ranking_scope_sql(alias="p"):
    """The WHERE-clause predicate for 'who counts' in rankings/leaderboards
    (NOT the same question as economy stats, which already count everyone,
    or the teleport-me human lookup, which always means real characters).
    With real players included, the installer's seeded admin/test account
    (Admin/AdminNinja/AdminSura/AdminSzaman, 500M gold each -- see the same
    exclusion for "yang w obiegu") would otherwise top every single
    category and bury any actual player under it."""
    if include_real_players_in_rankings():
        ref = (alias + ".") if alias else ""
        return ref + "name NOT IN ('[SA]Admin','Test','Admin','AdminNinja','AdminSura','AdminSzaman')"
    return bot_identity(alias)


def bot_ranking(kind, sort_by="avg"):
    base = ranking_scope_sql("p")
    if kind == "gold":
        return rows(f"SELECT p.id,p.name,p.level,p.gold,CONCAT(FORMAT(p.gold,0),' Yang') AS detail FROM player.player p WHERE {base} ORDER BY p.gold DESC,p.level DESC LIMIT 100")
    if kind == "weapon":
        return rows(f"""SELECT p.id,p.name,p.level,p.gold,i.vnum,COALESCE(ip.locale_name,CONCAT('VNUM ',i.vnum)) AS detail
            FROM player.player p LEFT JOIN player.item i ON i.owner_id=p.id AND i.window='EQUIPMENT' AND i.pos=4
            LEFT JOIN player.item_proto ip ON ip.vnum=i.vnum WHERE {base}
            ORDER BY MOD(COALESCE(i.vnum,0),10) DESC,i.vnum DESC,p.level DESC LIMIT 100""")
    if kind == "armor":
        return rows(f"""SELECT p.id,p.name,p.level,p.gold,i.vnum,COALESCE(ip.locale_name,CONCAT('VNUM ',i.vnum)) AS detail
            FROM player.player p LEFT JOIN player.item i ON i.owner_id=p.id AND i.window='EQUIPMENT' AND i.pos=0
            LEFT JOIN player.item_proto ip ON ip.vnum=i.vnum WHERE {base}
            ORDER BY MOD(COALESCE(i.vnum,0),10) DESC,i.vnum DESC,p.level DESC LIMIT 100""")
    if kind == "weapon30":
        weapon30_order = {
            "avg": "avg_damage DESC, skill_damage DESC, p.level DESC",
            "skill": "skill_damage DESC, avg_damage DESC, p.level DESC",
            "upgrade": "MOD(i.vnum,10) DESC, avg_damage DESC, skill_damage DESC, p.level DESC",
        }.get(sort_by, "avg_damage DESC, skill_damage DESC, p.level DESC")
        # avg_damage czyta APPLY_NORMAL_HIT_DAMAGE_BONUS (72), a
        # skill_damage APPLY_SKILL_DAMAGE_BONUS (71) - tak, jak nazywa je
        # common/length.h. Do 1.33.0 aliasy byly odwrotne, wiec ORDER BY
        # wybieral pierwsza setke po niewlasciwej kolumnie i poprawianie
        # samego sortowania w Pythonie nic by nie dalo.
        result = rows(f"""SELECT p.id,p.name,p.level,p.gold,i.vnum,COALESCE(ip.locale_name,CONCAT('VNUM ',i.vnum)) AS item_name,
            IF(GREATEST(CASE WHEN i.attrtype0={ATTR_SKILL_DAMAGE} THEN i.attrvalue0 ELSE -999 END,CASE WHEN i.attrtype1={ATTR_SKILL_DAMAGE} THEN i.attrvalue1 ELSE -999 END,CASE WHEN i.attrtype2={ATTR_SKILL_DAMAGE} THEN i.attrvalue2 ELSE -999 END,CASE WHEN i.attrtype3={ATTR_SKILL_DAMAGE} THEN i.attrvalue3 ELSE -999 END,CASE WHEN i.attrtype4={ATTR_SKILL_DAMAGE} THEN i.attrvalue4 ELSE -999 END,CASE WHEN i.attrtype5={ATTR_SKILL_DAMAGE} THEN i.attrvalue5 ELSE -999 END,CASE WHEN i.attrtype6={ATTR_SKILL_DAMAGE} THEN i.attrvalue6 ELSE -999 END)=-999,0,GREATEST(CASE WHEN i.attrtype0={ATTR_SKILL_DAMAGE} THEN i.attrvalue0 ELSE -999 END,CASE WHEN i.attrtype1={ATTR_SKILL_DAMAGE} THEN i.attrvalue1 ELSE -999 END,CASE WHEN i.attrtype2={ATTR_SKILL_DAMAGE} THEN i.attrvalue2 ELSE -999 END,CASE WHEN i.attrtype3={ATTR_SKILL_DAMAGE} THEN i.attrvalue3 ELSE -999 END,CASE WHEN i.attrtype4={ATTR_SKILL_DAMAGE} THEN i.attrvalue4 ELSE -999 END,CASE WHEN i.attrtype5={ATTR_SKILL_DAMAGE} THEN i.attrvalue5 ELSE -999 END,CASE WHEN i.attrtype6={ATTR_SKILL_DAMAGE} THEN i.attrvalue6 ELSE -999 END)) AS skill_damage,
            IF(GREATEST(CASE WHEN i.attrtype0={ATTR_AVG_DAMAGE} THEN i.attrvalue0 ELSE -999 END,CASE WHEN i.attrtype1={ATTR_AVG_DAMAGE} THEN i.attrvalue1 ELSE -999 END,CASE WHEN i.attrtype2={ATTR_AVG_DAMAGE} THEN i.attrvalue2 ELSE -999 END,CASE WHEN i.attrtype3={ATTR_AVG_DAMAGE} THEN i.attrvalue3 ELSE -999 END,CASE WHEN i.attrtype4={ATTR_AVG_DAMAGE} THEN i.attrvalue4 ELSE -999 END,CASE WHEN i.attrtype5={ATTR_AVG_DAMAGE} THEN i.attrvalue5 ELSE -999 END,CASE WHEN i.attrtype6={ATTR_AVG_DAMAGE} THEN i.attrvalue6 ELSE -999 END)=-999,0,GREATEST(CASE WHEN i.attrtype0={ATTR_AVG_DAMAGE} THEN i.attrvalue0 ELSE -999 END,CASE WHEN i.attrtype1={ATTR_AVG_DAMAGE} THEN i.attrvalue1 ELSE -999 END,CASE WHEN i.attrtype2={ATTR_AVG_DAMAGE} THEN i.attrvalue2 ELSE -999 END,CASE WHEN i.attrtype3={ATTR_AVG_DAMAGE} THEN i.attrvalue3 ELSE -999 END,CASE WHEN i.attrtype4={ATTR_AVG_DAMAGE} THEN i.attrvalue4 ELSE -999 END,CASE WHEN i.attrtype5={ATTR_AVG_DAMAGE} THEN i.attrvalue5 ELSE -999 END,CASE WHEN i.attrtype6={ATTR_AVG_DAMAGE} THEN i.attrvalue6 ELSE -999 END)) AS avg_damage
            FROM player.item i JOIN player.player p ON p.id=i.owner_id LEFT JOIN player.item_proto ip ON ip.vnum=i.vnum
            WHERE {base} AND ((i.vnum BETWEEN 290 AND 299) OR (i.vnum BETWEEN 1170 AND 1179) OR (i.vnum BETWEEN 2150 AND 2159) OR (i.vnum BETWEEN 3210 AND 3219) OR (i.vnum BETWEEN 5110 AND 5119) OR (i.vnum BETWEEN 7160 AND 7169))
            ORDER BY {weapon30_order} LIMIT 100""")
        # 71 is APPLY_SKILL_DAMAGE_BONUS and 72 is APPLY_NORMAL_HIT_DAMAGE_BONUS in
        # common/length.h, and the query names them so. A swap used to live
        # here, justified by "this build stores them the other way round" -
        # it does not, and the ranking showed the two columns exchanged.
        return sorted(
            result,
            key=lambda row: (
                (int(row.get("avg_damage") or 0), int(row.get("skill_damage") or 0), int(row.get("level") or 0)) if sort_by == "avg" else
                (int(row.get("skill_damage") or 0), int(row.get("avg_damage") or 0), int(row.get("level") or 0)) if sort_by == "skill" else
                (int(row.get("vnum") or 0) % 10, int(row.get("avg_damage") or 0), int(row.get("skill_damage") or 0), int(row.get("level") or 0))
            ),
            reverse=True,
        )
    if kind == "playtime":
        return rows(f"SELECT p.id,p.name,p.level,p.gold,p.playtime AS score,CONCAT(FLOOR(p.playtime/60),' h') AS detail FROM player.player p WHERE {base} ORDER BY p.playtime DESC,p.level DESC LIMIT 100")
    if kind == "bosses":
        return rows(f"""SELECT p.id,p.name,p.level,p.gold,COUNT(*) AS score,
            CONCAT(COUNT(*),' zabitych bossów · 7 dni') AS detail
            FROM log.log l JOIN player.player p ON p.id=l.who
            WHERE {base} AND l.how='BOSS_KILL' AND l.time >= NOW() - INTERVAL 7 DAY
            GROUP BY p.id,p.name ORDER BY score DESC,p.level DESC,p.name LIMIT 100""")
    if kind == "refine":
        # Same REFINE SUCCESS count character_stat_summary() already shows
        # on /player/ as "Pomyślne ulepszenia" -- all-time, not windowed,
        # so this ranking's numbers line up with that page's.
        return rows(f"""SELECT p.id,p.name,p.level,p.gold,COUNT(*) AS score,
            CONCAT(COUNT(*),' pomyślnych ulepszeń') AS detail
            FROM log.log l JOIN player.player p ON p.id=l.who
            WHERE {base} AND l.how='REFINE SUCCESS'
            GROUP BY p.id,p.name ORDER BY score DESC,p.level DESC,p.name LIMIT 100""")
    if kind == "refine_rate":
        # Ciekawostka, per operator's ask: % success needs a minimum sample
        # size, or a bot's very first-ever refine lands it at #1 forever
        # having never tried again. 20 attempts is comfortably above that.
        # Failure here is REMOVE (REFINE FAIL), not 'REFINE FAIL' -- checked
        # live for pid 84 (314 SUCCESS / 62 REMOVE (REFINE FAIL), matching
        # the 83.x% the operator saw on /player/): this engine has zero
        # 'REFINE FAIL' rows at all, every failed refine burns the item and
        # is logged only as the burn event.
        min_attempts = 20
        return rows(f"""SELECT p.id,p.name,p.level,p.gold,
            ROUND(100*SUM(l.how='REFINE SUCCESS')/COUNT(*),1) AS score,
            CONCAT(ROUND(100*SUM(l.how='REFINE SUCCESS')/COUNT(*),1),'%% (',SUM(l.how='REFINE SUCCESS'),'/',COUNT(*),' ulepszeń)') AS detail
            FROM log.log l JOIN player.player p ON p.id=l.who
            WHERE {base} AND l.how IN ('REFINE SUCCESS','REMOVE (REFINE FAIL)')
            GROUP BY p.id,p.name HAVING COUNT(*) >= {min_attempts}
            ORDER BY score DESC,COUNT(*) DESC,p.level DESC LIMIT 100""")
    if kind == "items":
        return rows(f"""SELECT p.id,p.name,p.level,p.gold,COUNT(i.id) AS score,CONCAT(COUNT(i.id),' przedmiotów') AS detail
            FROM player.player p LEFT JOIN player.item i ON i.owner_id=p.id AND i.window='INVENTORY'
            WHERE {base} GROUP BY p.id ORDER BY score DESC,p.level DESC LIMIT 100""")
    if kind == "horse":
        return rows(f"SELECT p.id,p.name,p.level,p.gold,p.horse_level AS score,CONCAT('Koń Lv ',p.horse_level) AS detail FROM player.player p WHERE {base} ORDER BY p.horse_level DESC,p.level DESC LIMIT 100")
    if kind == "biologist":
        missions = biologist_missions()
        marks = ",".join(["%s"] * len(missions))
        return rows(f"""SELECT p.id,p.name,p.level,p.gold,COUNT(DISTINCT q.szName) AS score,CONCAT(COUNT(DISTINCT q.szName),' / {len(missions)} misji') AS detail
            FROM player.player p LEFT JOIN player.quest q ON q.dwPID=p.id AND q.szName IN ({marks}) AND q.szState='__status' AND q.lValue=%s
            WHERE {base} GROUP BY p.id ORDER BY score DESC,p.level DESC LIMIT 100""", (*missions, BIOLOGIST_COMPLETE_STATE))
    # Ranking "hunting" usuniety razem z zakladka: levelup.quest nie dziala na
    # tej linii silnika, wiec zapytanie zwracalo sto rekordow z zerem. Gdyby
    # ktos wszedl ze starym ?type=hunting, kind nie ma go juz w kinds i strona
    # pokazuje domyslny ranking poziomu.
    if kind == "shops":
        keeper_ids = [pid for pid, state in live_statuses().items() if int(state.get("action") or 0) == 13]
        if not keeper_ids:
            return []
        placeholders = ",".join(["%s"] * len(keeper_ids))
        return rows(f"SELECT p.id,p.name,p.level,p.gold,'Stragan otwarty' AS detail FROM player.player p WHERE p.id IN ({placeholders}) ORDER BY p.level DESC LIMIT 100", keeper_ids)
    if kind == "skills":
        # Kazdy bot z profesja, a nie czterysta najwyzszych poziomem.
        # Ranking umiejetnosci posortowany najpierw po poziomie odpowiada
        # na inne pytanie: bot z trzydziestki z mistrzowska umiejetnoscia
        # stal pod czterystoma piecdziesiatkami bez zadnej i nie pokazywal
        # sie wcale. Punktowanie i tak jest w Pythonie, bo skill_level to
        # blob, wiec caly zbior musi wrocic.
        roster = rows(f"SELECT p.id,p.name,p.level,p.gold,p.job,p.skill_group,p.skill_level FROM player.player p WHERE {base} AND p.skill_group>0 ")
        for bot in roster:
            best = max(parse_skills(bot.get("skill_level"), bot.get("job"), bot.get("skill_group")), key=lambda skill: (3 if skill["rank"] == "P" else 2 if skill["rank"].startswith("G") else 1 if skill["rank"].startswith("M") else 0, skill["level"]), default=None)
            bot["score"] = (3 if best and best["rank"] == "P" else 2 if best and best["rank"].startswith("G") else 1 if best and best["rank"].startswith("M") else 0, best["level"] if best else 0)
            bot["detail"] = f"{best['name']} · {best['rank']}" if best else "Brak rozwiniętych umiejętności"
        return sorted(roster, key=lambda bot: (bot["score"], bot["level"]), reverse=True)[:100]
    if kind == "plus9":
        # Ktore vnumy sa sprzetem, rozstrzyga item_proto, a nie liczba:
        # "ponizej 12000" mialo odsiac materialy, a odsiewalo kazda tarcze
        # (13xxx) i cala bizuterie razem z nimi. type 1 to ITEM_WEAPON,
        # 2 to ITEM_ARMOR - dokladnie ten zbior, ktorego lancuch ulepszen
        # biegnie base+0..9.
        # window musi byc ograniczone do EQUIPMENT/INVENTORY: w SAFEBOX
        # owner_id to id KONTA, nie postaci (magazyn dzielony miedzy
        # postaciami), wiec bez tego warunku przedmiot ze skrytki trafial
        # do rankingu tej postaci, ktorej id przypadkiem zbieglo sie z
        # id konta wlasciciela skrytki.
        return rows(f"""SELECT p.id,p.name,p.level,p.gold,i.vnum,COALESCE(ip.locale_name,CONCAT('VNUM ',i.vnum)) AS detail
            FROM player.item i JOIN player.player p ON p.id=i.owner_id LEFT JOIN player.item_proto ip ON ip.vnum=i.vnum
            WHERE {base} AND i.window IN ('EQUIPMENT','INVENTORY') AND ip.type IN (1,2) AND MOD(i.vnum,10)=9 ORDER BY i.vnum DESC,p.level DESC LIMIT 100""")
    return rows(f"SELECT p.id,p.name,p.level,p.gold,p.level AS score,'Poziom' AS detail FROM player.player p WHERE {base} ORDER BY p.level DESC,p.exp DESC LIMIT 100")


def login_required(view):
    @wraps(view)
    def wrapped(*args, **kwargs):
        current = settings()
        if current.get("setup_complete") != "1":
            return redirect(url_for("setup"))
        if current.get("auth_enabled") != "1":
            return view(*args, **kwargs)
        if not session.get("seban_admin"):
            return redirect(url_for("login", next=request.full_path))
        return view(*args, **kwargs)
    return wrapped


def item_icon_url(vnum):
    try:
        value = int(vnum)
    except (TypeError, ValueError):
        return None
    # Most upgrade series use the same client icon for +0 through +9.
    # Prefer an explicit mapping, then fall back to the base VNUM safely.
    icon = ITEM_ICONS.get(str(value)) or ITEM_ICONS.get(str(value - value % 10))
    return url_for("static", filename=f"icons/{quote(icon)}") if icon else None


@app.context_processor
def globals_for_templates():
    tieru_url = os.environ.get("TIERU_PANEL_URL", "http://127.0.0.1:7788")
    item_icon = item_icon_url
    current_settings = settings()
    def job_name(job):
        return class_profile(job)["name"]
    def class_portrait(job):
        return url_for("static", filename=f"class-portraits/{class_profile(job)['portrait']}")
    def empire_flag(empire):
        flag = empire_flag_path(empire)
        return url_for("static", filename=f"empires/{flag}") if flag else ""
    return {"tieru_url": tieru_url, "panel_brand": current_settings.get("panel_name", "Metin2 Singleplayer"), "settings": current_settings, "map_name": map_name, "item_icon": item_icon, "job_name": job_name, "class_profile": class_profile, "class_portrait": class_portrait, "empire_info": empire_info, "empire_flag": empire_flag}
@app.route("/login", methods=["GET", "POST"])
def login():
    current = settings()
    if current.get("auth_enabled") != "1":
        return redirect(url_for("dashboard"))
    if request.method == "POST":
        password_hash = current.get("auth_password_hash", "")
        if password_hash and check_password_hash(password_hash, request.form.get("password", "")):
            session.clear()
            session["seban_admin"] = True
            session.permanent = True
            return redirect(request.args.get("next") or url_for("dashboard"))
        flash("Nieprawidłowe hasło.", "error")
    return render_template("login.html")


@app.post("/logout")
def logout():
    session.clear()
    return redirect(url_for("login"))


@app.route("/setup", methods=["GET", "POST"])
def setup():
    current = settings()
    if current.get("setup_complete") == "1":
        return redirect(url_for("dashboard"))
    if request.method == "POST":
        values, error = validate_display_settings(request.form)
        password = request.form.get("panel_password", "")
        enable_auth = request.form.get("auth_enabled") == "1"
        if enable_auth and len(password) < 8:
            error = "Hasło panelu musi mieć co najmniej 8 znaków."
        if error:
            flash(error, "error")
        else:
            values.update({"setup_complete": "1", "auth_enabled": "1" if enable_auth else "0", "auth_password_hash": generate_password_hash(password) if enable_auth else ""})
            write_settings(values)
            if enable_auth:
                session["seban_admin"] = True
            flash("Konfiguracja została zapisana.")
            return redirect(url_for("dashboard"))
    return render_template("setup.html", current=current)


@app.route("/")
@login_required
def dashboard():
    totals = one("""
        SELECT
          (SELECT COUNT(*) FROM player.player) AS characters,
          (SELECT COUNT(*) FROM account.account) AS accounts,
          (SELECT COUNT(*) FROM player.item) AS item_stacks,
          (SELECT COALESCE(SUM(gold),0) FROM player.player WHERE name NOT IN ('[SA]Admin','Test','Admin','AdminNinja','AdminSura','AdminSzaman')) AS yang
    """)
    bots = one("SELECT COUNT(*) AS count FROM player.player WHERE account_id BETWEEN 4 AND 1003")
    # The collector creates this table with its first snapshot; before that -
    # the first minutes of a fresh installation - the dashboard has no host
    # metrics to show, not an error to raise.
    try:
        system = one("SELECT * FROM player.web_seban_system_snapshot ORDER BY captured_at DESC LIMIT 1")
    except pymysql.MySQLError:
        system = {}
    map_rows = live_map_counts()
    for row in map_rows:
        row["name"] = map_name(row["map_index"])
    # "Boty według map" tile alternates with this second dataset (JS-driven,
    # see dashboard-charts.js) instead of a 4th tile -- three donut/carousel
    # tiles already fill the row edge-to-edge; a 4th would cramp all of them.
    # Same per-(map,empire) shape as economy_shops()'s by_map, so the exact
    # same flag+map-code bar chart plugin can be reused here, just smaller.
    shop_map_rows = []
    # Created by the collector's first snapshot too, so the same "not yet"
    # applies: a missing table is an empty chart, never a 500.
    try:
        shop_snapshot_latest = one("SELECT MAX(captured_at) AS captured_at FROM player.web_seban_shop_snapshot").get("captured_at")
    except pymysql.MySQLError:
        shop_snapshot_latest = None
    if shop_snapshot_latest:
        raw_shop_map = rows("""SELECT map_index, empire, shop_count FROM player.web_seban_shop_snapshot
          WHERE captured_at=%s ORDER BY empire, shop_count DESC""", (shop_snapshot_latest,))
        shop_map_rows = [{"map_index": r["map_index"], "empire": int(r["empire"]), "map_short": map_short_code(r["map_index"]), "shop_count": int(r["shop_count"])} for r in raw_shop_map if int(r["shop_count"]) > 0]
    top = rows("SELECT id, name, level, exp, job, map_index, playtime FROM player.player WHERE " + ranking_scope_sql("") + " ORDER BY level DESC, exp DESC LIMIT 10")
    global_top_id = top[0]["id"] if top else None
    live = live_statuses()
    live_roster = live_bots()
    try:
        bot_guilds = one("""SELECT COUNT(*) AS count FROM player.guild g
                           JOIN player.player p ON p.id=g.master WHERE """ + BOT_IS).get("count", 0)
    except pymysql.MySQLError:
        bot_guilds = 0
    restart_status = read_rate_status()
    restart_time = restart_status.get("time")
    try:
        restart_label = datetime.fromtimestamp(int(restart_time)).strftime("%d.%m.%Y, %H:%M:%S")
    except (TypeError, ValueError, OSError):
        restart_label = "Brak danych"
    release_status = playerbots_release_status()
    world_summary = {
        "bots": len(live_roster),
        "average_level": round(sum(int(bot.get("level") or 0) for bot in live_roster) / len(live_roster), 1) if live_roster else 0,
        "party_bots": sum(1 for bot in live_roster if bot.get("in_party")),
        "max_level": max((int(bot.get("level") or 0) for bot in live_roster), default=0),
        "horse_average": round(sum(int(bot.get("horse_level") or 0) for bot in live_roster) / len(live_roster), 1) if live_roster else 0,
        "horse_max": max((int(bot.get("horse_level") or 0) for bot in live_roster), default=0),
        "guilds": bot_guilds,
        "last_restart": restart_label,
        "version": release_status["installed"],
        "release": release_status,
        "rates": read_rates(),
    }
    for bot in top:
        if bot["id"] in live:
            bot["map_index"] = live[bot["id"]]["map_index"]
    quick_rankings = []
    quick_rankings.append({"title": "Poziom", "subtitle": "najwyższe poziomy", "items": [{"id": row["id"], "name": row["name"], "value": f"Lv {row['level']}"} for row in top]})
    playtime = bot_ranking("playtime")[:10]
    quick_rankings.append({"title": "Czas gry", "subtitle": "najdłużej online", "items": [{"id": row["id"], "name": row["name"], "value": row["detail"]} for row in playtime]})
    gold = bot_ranking("gold")[:10]
    quick_rankings.append({"title": "Yang", "subtitle": "najwięcej przy postaci", "items": [{"id": row["id"], "name": row["name"], "value": f"{int(row.get('gold') or 0):,}".replace(",", " ")} for row in gold]})
    weapon30 = bot_ranking("weapon30")[:10]
    quick_rankings.append({"title": "Broń 30 Lv", "subtitle": "średnie / umiejętności", "items": [{"id": row["id"], "name": row["name"], "value": f"Śr. {int(row.get('avg_damage') or 0)}% · Um. {int(row.get('skill_damage') or 0)}%"} for row in weapon30]})
    metins = rows("""SELECT p.id,p.name,COUNT(*) AS score FROM log.log l JOIN player.player p ON p.id=l.who
                     WHERE """ + ranking_scope_sql("p") + """ AND l.how='STONE_KILL' AND l.time >= NOW() - INTERVAL 7 DAY
                     GROUP BY p.id,p.name ORDER BY score DESC,p.name LIMIT 10""")
    quick_rankings.append({"title": "Metiny", "subtitle": "rozbite · ostatnie 7 dni", "items": [{"id": row["id"], "name": row["name"], "value": f"{int(row['score'])} szt."} for row in metins]})
    bosses = bot_ranking("bosses")[:10]
    quick_rankings.append({"title": "Bossy", "subtitle": "zabite · ostatnie 7 dni", "items": [{"id": row["id"], "name": row["name"], "value": f"{int(row['score'])} szt."} for row in bosses]})
    # "Ryby" used to sit here (LIKE '%ryb%' on log.log.what) but the engine
    # never logs a catch anywhere -- confirmed zero matching rows on live
    # data, matching character_stat_summary()'s note that fishing has no
    # server-side record at all. Swapped for Pomyślne ulepszenia, which does
    # have real data (same REFINE SUCCESS count /player/ already shows).
    refine = bot_ranking("refine")[:10]
    quick_rankings.append({"title": "Pomyślne ulepszenia", "subtitle": "łącznie, całościowo", "items": [{"id": row["id"], "name": row["name"], "value": f"{int(row['score'])} szt."} for row in refine]})
    refine_rate = bot_ranking("refine_rate")[:10]
    quick_rankings.append({"title": "Skuteczność ulepszeń", "subtitle": "% sukcesu · min. 20 prób", "items": [{"id": row["id"], "name": row["name"], "value": f"{row['score']}%"} for row in refine_rate]})
    ranking_ids = {item["id"] for ranking in quick_rankings for item in ranking["items"]}
    if ranking_ids:
        placeholders = ",".join(["%s"] * len(ranking_ids))
        jobs_by_id = {row["id"]: row["job"] for row in rows("SELECT id,job FROM player.player WHERE id IN (" + placeholders + ")", list(ranking_ids))}
        for quick_ranking in quick_rankings:
            for item in quick_ranking["items"]:
                item["job"] = jobs_by_id.get(item["id"], 0)
    return render_template("dashboard.html", totals=totals, bots=bots.get("count", 0), system=system, map_rows=map_rows, shop_map_rows=shop_map_rows, top=top, global_top_id=global_top_id, quick_rankings=quick_rankings, world_summary=world_summary, panel_version=PANEL_VERSION, latest_changelog=changelog_entries()[:1])
@app.route("/players")
@login_required
def players():
    query = request.args.get("q", "").strip()
    sql = "SELECT id, name, level, job, map_index, gold, playtime, last_play FROM player.player"
    args = []
    if query:
        sql += " WHERE name LIKE %s OR id=%s"
        args = [f"%{query}%", query if query.isdigit() else -1]
    sql += " ORDER BY level DESC, exp DESC LIMIT 250"
    roster, live = rows(sql, args), live_statuses()
    for character in roster:
        state = live.get(character["id"])
        character["map_live"] = bool(state)
        if state:
            character["map_index"] = state["map_index"]
    return render_template("players.html", players=roster, query=query)


@app.route("/guilds")
@login_required
def guilds():
    query = request.args.get("q", "").strip()
    roster, written_at = guild_statuses()
    if query:
        needle = query.casefold()
        roster = [g for g in roster if needle in g["name"].casefold() or needle in g["master"].casefold()]
    next_wars = {}
    for guild in roster:
        empire, seconds = guild.get("empire", 0), guild.get("next_war_in_s")
        if empire not in EMPIRES or seconds is None:
            continue
        old = next_wars.get(empire)
        if old is None or (seconds >= 0 and (old < 0 or seconds < old)):
            next_wars[empire] = seconds
    summary = {"guilds": len(roster), "online": sum(g["online"] for g in roster),
               "wars": sum(1 for g in roster if g["war_with"]),
               "exp": sum(g["exp_offered"] for g in roster)}
    return render_template("guilds.html", guilds=roster, query=query, summary=summary,
                           next_wars=[{"empire": empire, "text": guild_war_text(seconds)} for empire, seconds in sorted(next_wars.items())],
                           status_written_at=datetime.fromtimestamp(written_at).strftime("%H:%M") if written_at else None)


@app.route("/guild/<int:guild_id>")
@login_required
def guild(guild_id):
    details = one("""SELECT g.id,g.name,g.level,g.exp,g.sp,g.win,g.draw,g.loss,g.ladder_point,g.gold,
                     leader.id AS leader_id,leader.name AS leader_name,leader.level AS leader_level,
                     COUNT(gm.pid) AS member_count
                     FROM player.guild g
                     LEFT JOIN player.player leader ON leader.id=g.master
                     LEFT JOIN player.guild_member gm ON gm.guild_id=g.id
                     WHERE g.id=%s
                     GROUP BY g.id,g.name,g.level,g.exp,g.sp,g.win,g.draw,g.loss,g.ladder_point,g.gold,leader.id,leader.name,leader.level""", (guild_id,))
    if not details:
        abort(404)
    members = rows("""SELECT gm.pid,gm.grade,gm.is_general,gm.offer,p.name,p.level,p.job,p.map_index,p.playtime
                    FROM player.guild_member gm LEFT JOIN player.player p ON p.id=gm.pid
                    WHERE gm.guild_id=%s
                    ORDER BY (gm.pid=%s) DESC,gm.grade ASC,p.level DESC,p.name ASC""", (guild_id, details["leader_id"] or 0))
    return render_template("guild.html", guild=details, members=members)


def character_stat_summary(pid):
    """The subset of the client's Y-panel (character records) this engine
    actually persists server-side. Verified against a live character
    ([GA]Seban) against the exact numbers its own client showed, not assumed
    from another Metin2 build:
      - player.player carries no per-character counters at all (its full,
        47-column schema has only current gold, no kill/PVP/gather counts).
      - The player schema's other 52 tables were checked too (including
        `quest`'s per-character flags for this pid) -- nothing named or
        shaped like a battle-record table exists anywhere in it.
      - log.log has exactly 50 distinct `how` values on this world's full
        history, all checked. The ones below matched their client-shown
        numbers EXACTLY on a live test (bosses, metins, deaths_by_mob,
        refine_success, refine_burned, and both pvp_kills/pvp_deaths below).
        There is still no MOB_KILL, duel, mining/fishing/flower, dungeon-
        clear, quest-book, or damage-record `how` at all -- confirmed absent,
        not merely unmatched for one character.
      - yang_earned (GET_GOLD only) is a known UNDER-count: it read 1,379,865
        against a client-shown 6,952,633 for the same character. GM-granted
        items/gold and a few other `how` values touch this character's
        wallet without logging a parseable amount anywhere nearby in time,
        so the gap could not be closed without guessing -- shown as a
        (labelled) partial total, not corrected upward by assumption.
      - SHOP_SELL/NPC_SELL's hint packs item/seller into a free-text string
        with no price field anywhere in the row (checked NPC_SELL rows AND
        every log entry in the same second, looking for a companion
        GET_GOLD -- there isn't one), so "yang from NPC sales" has no
        server-side source at all on this build, not just an unparsed one.
    Whatever is missing is surfaced by simply not being a key in the
    returned dict -- character_stats.html decides how to render that gap,
    this function never fabricates a zero for something it cannot see.
    """
    row = one("""SELECT
        SUM(how='BOSS_KILL') AS bosses,
        SUM(how='STONE_KILL') AS metins,
        SUM(how='DEAD_BY_NPC') AS deaths_by_mob,
        SUM(how='DEAD_BY_PC') AS pvp_deaths,
        SUM(how='REFINE SUCCESS') AS refine_success,
        SUM(how='REMOVE (REFINE FAIL)') AS refine_burned,
        COALESCE(SUM(CASE WHEN how='GET_GOLD' THEN what ELSE 0 END),0) AS yang_earned
      FROM log.log WHERE who=%s""", (pid,))
    stats = {key: int(value or 0) for key, value in row.items()} if row else {}
    # DEAD_BY_PC logs the loser as `who` and the killer's pid as `what` --
    # a PVP win for this pid is therefore a second query keyed the other way
    # round, not another column of the row above.
    kills = one("SELECT COUNT(*) AS n FROM log.log WHERE how='DEAD_BY_PC' AND what=%s", (pid,))
    stats["pvp_kills"] = int(kills.get("n") or 0) if kills else 0
    return stats


# Curated subset of log.log's `how` values that make an "equipment history"
# instead of noise: log.log holds thousands of GET/SET_SOCKET/GET_GOLD rows
# per bot, which drowned out the handful of equipment/trade events an
# operator actually wants -- matches Tieru's own /api/bot_gear_history on
# 7788 (audit, 2026-09-14), translated to Polish only (this panel has no
# language switcher).
GEAR_HISTORY_HOWS = {
    "REFINE SUCCESS": ("refine-ok", "Ulepszenie udane"),
    "REFINE FAIL": ("refine-fail", "Ulepszenie nieudane"),
    "REMOVE (REFINE FAIL)": ("burned", "Spalone przy ulepszaniu"),
    "REFINE FISH_ROD SUCCESS": ("refine-ok", "Wędka ulepszona"),
    "REFINE FISH_ROD FAIL": ("refine-fail", "Wędka nieulepszona"),
    "PLAYERBOT_EQUIP": ("equip", "Założone"),
    "PLAYERBOT_GIFT_OUT": ("gift-out", "Podarowane"),
    "PLAYERBOT_GIFT_IN": ("gift-in", "Dostane w prezencie"),
    "PLAYERBOT_STALL_SOLD": ("stall-sold", "Sprzedane na straganie"),
    "SHOP_BUY": ("bought", "Kupione na straganie"),
    "PLAYERBOT_SHOP_SELL": ("vendor", "Sprzedane handlarzowi"),
    "PLAYERBOT_BONUS": ("bonus", "Zużyte na przemianę bonusów"),
    "PLAYERBOT_BONUS_ADD": ("bonus", "Dodano bonus (Wzmocnienie)"),
    "PLAYERBOT_BONUS_CHANGE": ("bonus", "Zmieniono bonusy (Zmiana)"),
    "PLAYERBOT_BONUS_MARBLE": ("bonus", "Dodano 5. bonus (Marmur)"),
    "SAFEBOX PUT": ("safebox", "Do magazynu"),
    "SAFEBOX GET": ("safebox", "Z magazynu"),
    "MOONLIGHT_GET": ("get", "Ze Szkatułki Blasku"),
    "EXCHANGE_TAKE": ("gift-in", "Z wymiany"),
    "EXCHANGE_GIVE": ("gift-out", "Oddane w wymianie"),
}


def bot_gear_history(pid, limit=60):
    hows = list(GEAR_HISTORY_HOWS.keys())
    marks = ",".join(["%s"] * len(hows))
    raw = rows(f"""SELECT l.time, l.how, l.hint, l.vnum, i.socket0 FROM log.log l
      LEFT JOIN player.item i ON i.id = l.what
      WHERE l.who=%s AND l.how IN ({marks}) ORDER BY l.time DESC LIMIT %s""", [pid] + hows + [limit])
    result = []
    for r in raw:
        how = game_text(r["how"])
        kind, label = GEAR_HISTORY_HOWS.get(how, ("other", how))
        vnum = int(r["vnum"] or 0)
        socket0 = int(r["socket0"] or 0) if vnum in SKILLBOOK_VNUMS else 0
        hint = game_text(r["hint"]).strip()
        detail = ""
        if how == "PLAYERBOT_GIFT_OUT":
            detail = "→ " + hint
        elif how == "PLAYERBOT_GIFT_IN":
            detail = "← " + hint
        elif how == "PLAYERBOT_STALL_SOLD":
            match = SALE_HINT_RE.match(hint)
            if match:
                detail = f"x{match.group(2)} za " + "{:,}".format(int(match.group(3))).replace(",", " ") + " yang"
        elif how == "PLAYERBOT_EQUIP":
            parts = hint.split()
            if len(parts) >= 4 and parts[3].isdigit() and int(parts[3]) > 0:
                detail = "zamiast " + _item_display_name(int(parts[3]))
        elif how in ("SAFEBOX PUT", "SAFEBOX GET"):
            parts = hint.rsplit(" ", 1)
            if len(parts) == 2 and parts[1].isdigit() and int(parts[1]) > 1:
                detail = "x" + parts[1]
        result.append({
            "time": r["time"].strftime("%d.%m %H:%M") if hasattr(r["time"], "strftime") else str(r["time"]),
            "kind": kind, "label": label,
            "item": _item_display_name(vnum, socket0) if vnum else "",
            "detail": detail,
        })
    return result


def bot_offline_shop(pid):
    """Data straight from IkarusShop's own tables -- there is no separate
    price/listing table for offline shops on this engine (confirmed against
    a live shop while building the /economy/shops feed): ikashop_offlineshop
    is the stall itself (map, x, y, banner name), player.item WHERE
    window='IKASHOP_OFFLINESHOP' is the listing, and each offer's yang price
    lives in that item's own ikashop_data JSON column."""
    shop = one("SELECT map, x, y, name, is_premium FROM player.ikashop_offlineshop WHERE owner=%s", (pid,))
    if not shop:
        return None
    offers = rows("""SELECT vnum, count, pos, socket0,
        CAST(JSON_UNQUOTE(JSON_EXTRACT(ikashop_data,'$.yang')) AS UNSIGNED) AS price
      FROM player.item WHERE owner_id=%s AND window='IKASHOP_OFFLINESHOP' ORDER BY pos""", (pid,))
    for offer in offers:
        offer["item_name"] = _item_display_name(offer["vnum"], offer.get("socket0"))
        offer["icon_url"] = item_icon_url(offer["vnum"])
        offer["price"] = int(offer.get("price") or 0)
    return {
        "name": game_text(shop["name"]) or "Bez nazwy", "map_index": int(shop["map"]), "map_name": map_name(shop["map"]),
        "x": int(shop["x"]), "y": int(shop["y"]), "is_premium": bool(shop["is_premium"]), "offers": offers,
    }


def bot_live_logs(name, limit=80):
    """Tail of the live game core's own syslogs, filtered to lines naming
    this bot -- same source (channel1/*/syslog) and word-boundary matching
    as Tieru's own /api/bot_logs on 7788, so a short name doesn't also
    match a longer sibling's (botgrom vs botgrom2)."""
    if not name:
        return []
    name_re = re.compile(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"(?![A-Za-z0-9_])", re.IGNORECASE)
    matched = []
    for path in Path("/").glob(BOT_SYSLOG_GLOB.lstrip("/")):
        try:
            with open(path, "r", encoding="latin-1", errors="ignore") as f:
                lines = f.readlines()
            recent = lines[-800:] if len(lines) > 800 else lines
            matched.extend(line.strip() for line in recent if name_re.search(line))
        except OSError:
            continue
    return matched[-limit:]


@app.route("/api/bot-logs/<int:pid>")
@login_required
def api_bot_logs(pid):
    character = one("SELECT name FROM player.player WHERE id=%s", (pid,))
    if not character:
        return {"ok": False, "logs": []}, 404
    return {"ok": True, "logs": bot_live_logs(character["name"])}


@app.route("/api/admin/teleport-me", methods=["POST"])
@login_required
def api_admin_teleport_me():
    """Moves whichever GM/human character is actually online right now to a
    bot's current position -- same one-click 'teleport me' the operator uses
    on Tieru's panel (7788), reusing the exact queue our own web_admin.quest
    already polls for item/gold grants (see item_grants.py). The panel
    cannot ask the database who is online (last_play only updates on save,
    minutes later), so every recently-active human character gets a queued
    WARP and whichever one is truly in the game answers first; the rest are
    withdrawn immediately so nobody is moved later for a click made now."""
    data = request.get_json(silent=True) or {}
    pid = int(data.get("pid") or 0)
    if data.get("x") and data.get("y"):
        # Explicit coordinates -- e.g. a shop's own stall position, which can
        # outlive the bot going offline (IkarusShop keeps the stall open).
        target_x, target_y = int(data["x"]), int(data["y"])
    else:
        live = live_statuses().get(pid)
        if not live:
            return {"ok": False, "error": "bot_offline"}
        target_x, target_y = int(live["x"]), int(live["y"])
    names = [r["name"] for r in rows(
        "SELECT name FROM player.player WHERE NOT (" + BOT_IS_BARE + ")"
        " AND last_play >= NOW() - INTERVAL 7 DAY ORDER BY last_play DESC LIMIT 8")]
    if not names:
        return {"ok": False, "error": "no_human_player"}
    for name in names:
        rows("INSERT INTO player.web_admin_queue (player_name,cmd,arg1,arg2) VALUES (%s,'WARP',%s,%s)",
             (name, str(target_x), str(target_y)))
    ids = {r["id"]: r["player_name"] for r in rows(
        "SELECT id, player_name FROM player.web_admin_queue WHERE cmd='WARP' AND status='pending'"
        " AND arg1=%s AND arg2=%s AND player_name IN (" + ",".join(["%s"] * len(names)) + ")",
        [str(target_x), str(target_y)] + names)}
    moved, status = None, "timeout"
    deadline = time.time() + 6.0
    while time.time() < deadline and moved is None:
        time.sleep(0.6)
        for r in rows("SELECT id, player_name, status FROM player.web_admin_queue WHERE id IN (" +
                       ",".join(["%s"] * len(ids)) + ")", list(ids.keys())):
            if r["status"] not in ("pending", None):
                moved, status = r["player_name"], r["status"]
                break
    rows("DELETE FROM player.web_admin_queue WHERE status='pending' AND id IN (" +
         ",".join(["%s"] * len(ids)) + ")", list(ids.keys()))
    if moved is None:
        return {"ok": False, "error": "player_offline", "tried": names}
    return {"ok": status == "done", "status": status, "name": moved, "x": target_x, "y": target_y}


@app.route("/player/<int:pid>")
@login_required
def player(pid):
    character = one("SELECT p.id,p.account_id,p.name,p.level,p.job,p.exp,p.gold,p.hp,p.mp,p.x,p.y,p.horse_level,p.alignment,p.st,p.ht,p.dx,p.iq,p.stat_point,p.skill_point,p.skill_group,p.skill_level,p.map_index,p.playtime,p.last_play,"
      "a.cash,a.silver_expire,a.gold_expire,a.safebox_expire,a.autoloot_expire,a.fish_mind_expire,a.marriage_fast_expire,a.money_drop_rate_expire,a.shop_expire,a.premium_expire,"
      + EMPIRE_EXPR + " AS empire FROM player.player p LEFT JOIN account.account a ON a.id=p.account_id LEFT JOIN player.player_index pi ON pi.id=p.account_id WHERE p.id=%s", (pid,))
    if not character:
        abort(404)
    live = live_statuses().get(pid)
    if live:
        character.update(live)
        character["personality"] = personality_label(live)
        character["charakter"] = live_label("personality", live.get("personality")) if live.get("persona") is not None else ""
        character["mood"] = mood_label(live)
        character["hold"] = f"blokada expa na {live['lock_level']} lvl" if live.get("persona") is not None and live.get("lock_level") else ""
        character["ambition"] = live_label("ambition", live.get("ambition"))
        character["goal"] = live_label("goal", live.get("goal"))
        character["action"] = live.get("status") or live_label("action", live.get("action"))
    else:
        character.update({"personality": "Bot offline", "ambition": "—", "goal": "—", "action": "—"})
    character["job_name"] = class_profile(character.get("job"))["name"]
    character["class_profile"] = class_profile(character.get("job"))
    character["experience"] = experience_progress(character.get("level"), character.get("exp"))
    character["honor"] = honor_rank(character.get("alignment"))
    character["honor"]["css"] = {"Rycerski": "knightly", "Szlachetny": "noble", "Dobry": "good", "Przyjazny": "friendly", "Neutralny": "neutral", "Agresywny": "aggressive", "Nieuczciwy": "dishonest", "Złośliwy": "malicious", "Okrutny": "cruel"}[character["honor"]["title"]]
    character["cash"] = int(character.get("cash") or 0)
    # One row per active *_expire column, human label first (see PREMIUM_TYPES)
    # so this always matches what "Nadaj VIP" itself offers -- no separate list
    # of names to keep in sync.
    now = datetime.now()
    character["premiums"] = [
        {"label": label, "expires": character.get(column)}
        for _type, label in PREMIUM_TYPES
        for column in [PREMIUM_COLUMNS[_type]]
        # shop_expire's schema default is the invalid zero-date
        # '0000-00-00 00:00:00', which the driver cannot represent as a
        # datetime and hands back as that literal string instead -- never an
        # active grant, but not directly comparable to `now` either.
        if isinstance(character.get(column), datetime) and character[column] > now
    ]
    character["playtime_hours"] = int(character.get("playtime") or 0) // 60
    character["playtime_minutes"] = int(character.get("playtime") or 0) % 60
    marriage = one("""SELECT p2.name AS partner_name FROM player.marriage m
      JOIN player.player p2 ON p2.id = IF(m.pid1=%s, m.pid2, m.pid1)
      WHERE (m.pid1=%s OR m.pid2=%s) AND m.is_married=1""", (pid, pid, pid))
    character["marriage_partner"] = marriage.get("partner_name") if marriage else None
    guild = one("""SELECT g.name AS guild_name, COALESCE(gg.name, '') AS grade_name FROM player.guild_member gm
      JOIN player.guild g ON g.id=gm.guild_id
      LEFT JOIN player.guild_grade gg ON gg.guild_id=gm.guild_id AND gg.grade=gm.grade
      WHERE gm.pid=%s""", (pid,))
    character["guild_name"] = guild.get("guild_name") if guild else None
    character["guild_grade"] = game_text(guild.get("grade_name")) if guild else None
    character["max_hp"] = max(int(character.get("max_hp") or 0), int(character.get("hp") or 0), 1)
    # The live Playerbots feed exposes exact max HP.  The original server
    # schema does not persist max MP, so an offline character is shown as a
    # current-value bar until it is next observed live.
    character["max_mp"] = max(int(character.get("max_mp") or 0), int(character.get("mp") or 0), 1)
    character["hp_percent"] = min(100, round(int(character.get("hp") or 0) * 100 / character["max_hp"], 1))
    character["mp_percent"] = min(100, round(int(character.get("mp") or 0) * 100 / character["max_mp"], 1))
    skill_raw = character.pop("skill_level", b"")
    character["skills"] = parse_skills(skill_raw, character.get("job"), character.get("skill_group"))
    character["passive_skills"] = parse_passive_skills(skill_raw)
    items = rows("""
      SELECT i.id, i.vnum, i.count, i.window, i.pos, i.socket0,i.socket1,i.socket2,
      i.attrtype0,i.attrvalue0,i.attrtype1,i.attrvalue1,i.attrtype2,i.attrvalue2,i.attrtype3,i.attrvalue3,i.attrtype4,i.attrvalue4,i.attrtype5,i.attrvalue5,i.attrtype6,i.attrvalue6,
      p.applytype0,p.applyvalue0,p.applytype1,p.applyvalue1,p.applytype2,p.applyvalue2,p.size AS item_size,COALESCE(p.locale_name, CONCAT('VNUM ', i.vnum)) AS item_name
      FROM player.item i LEFT JOIN player.item_proto p ON p.vnum=i.vnum WHERE i.owner_id=%s
      ORDER BY i.window, i.pos LIMIT 250
    """, (pid,))
    safebox = rows("""
      SELECT i.id,i.vnum,i.count,i.window,i.pos,i.socket0,i.socket1,i.socket2,
      i.attrtype0,i.attrvalue0,i.attrtype1,i.attrvalue1,i.attrtype2,i.attrvalue2,i.attrtype3,i.attrvalue3,i.attrtype4,i.attrvalue4,i.attrtype5,i.attrvalue5,i.attrtype6,i.attrvalue6,
      p.applytype0,p.applyvalue0,p.applytype1,p.applyvalue1,p.applytype2,p.applyvalue2,p.size AS item_size,COALESCE(p.locale_name,CONCAT('VNUM ',i.vnum)) AS item_name
      FROM player.item i LEFT JOIN player.item_proto p ON p.vnum=i.vnum WHERE i.owner_id=%s AND i.window='SAFEBOX' ORDER BY i.pos LIMIT 180
    """, (character["account_id"] if "account_id" in character else one("SELECT account_id FROM player.player WHERE id=%s", (pid,)).get("account_id"),))
    equipment, inventory = {}, []
    # EWearPositions from Server/common/length.h. The database stores these
    # offsets directly in EQUIPMENT (rather than their client offset +90).
    equipment_slots = {
        0: "body", 1: "head", 2: "foots", 3: "wrist", 4: "weapon",
        5: "neck", 6: "ear", 7: "unique1", 8: "unique2", 9: "arrow",
        10: "shield", 23: "belt",
    }
    for item in [*items, *safebox]:
        item["item_name"] = resolve_item_display_name(item["vnum"], item.get("socket0"), game_text(item["item_name"]))
        item["item_size"] = max(1, min(3, int(item.get("item_size") or 1)))
        item["base_stats"] = item_base_stats(item["vnum"])
        item["bonuses"] = [apply_text(item.get(f"applytype{i}"), item.get(f"applyvalue{i}")) for i in range(3) if item.get(f"applytype{i}") and item.get(f"applyvalue{i}")]
        item["bonuses"] += [apply_text(item.get(f"attrtype{i}"), item.get(f"attrvalue{i}")) for i in range(7) if item.get(f"attrtype{i}") and item.get(f"attrvalue{i}")]
        if item["window"] == "EQUIPMENT" and item["pos"] in equipment_slots:
            equipment[equipment_slots[item["pos"]]] = item
        elif item["window"] == "INVENTORY" and int(item["pos"] or 0) < INVENTORY_PAGE_SIZE * INVENTORY_PAGES:
            inventory.append(item)
    socket_vnums = sorted({int(item.get(f"socket{i}") or 0) for item in [*items, *safebox] for i in range(3) if int(item.get(f"socket{i}") or 0) > 0})
    stone_defs = {}
    if socket_vnums:
        marks = ",".join(["%s"] * len(socket_vnums))
        for stone in rows("SELECT vnum,COALESCE(locale_name,CONCAT('VNUM ',vnum)) AS item_name,applytype0,applyvalue0,applytype1,applyvalue1,applytype2,applyvalue2 FROM player.item_proto WHERE vnum IN (" + marks + ")", socket_vnums):
            stone_defs[int(stone["vnum"])] = {"name": game_text(stone["item_name"]), "bonuses": [apply_text(stone.get(f"applytype{i}"), stone.get(f"applyvalue{i}")) for i in range(3) if stone.get(f"applytype{i}") and stone.get(f"applyvalue{i}")]}
    for item in [*items, *safebox]:
        # A Skill Book's socket0 is the taught skill's vnum, not a gem --
        # looking it up in item_proto as a "stone" was matching unrelated
        # items by coincidence (e.g. a sword showing up in a book's tooltip).
        if int(item["vnum"] or 0) in SKILLBOOK_VNUMS:
            item["stones"] = []
        else:
            item["stones"] = [stone_defs[vnum] for vnum in (int(item.get(f"socket{i}") or 0) for i in range(3)) if vnum in stone_defs]
    gear_history = bot_gear_history(pid)
    offline_shop = bot_offline_shop(pid)
    character_stats = character_stat_summary(pid)
    # Client uiinventory.py: a page every 45 cells, page I at slot 0.
    return render_template("player.html", character=character, equipment=equipment, inventory=inventory, safebox=safebox, inventory_pages=INVENTORY_PAGES, has_safebox=bool(safebox), gear_history=gear_history, offline_shop=offline_shop, character_stats=character_stats)


# VIP and "Dragon Coins" both turned out to be real, already-working engine
# features, not something this panel needs to invent: CItemShopManager::
# AddVIP (server/game/src/itemshop_manager.cpp) extends one of nine
# account.account "*_expire" columns, and account.cash is the exact balance
# the already-running ItemShop (docker-compose.yml's own comment: "Dragon
# Coins (account.cash) and Dragon Marks (account.mileage)") already spends
# in-game. Granting through the panel writes the same columns the same way
# the game itself does, instead of a parallel panel-only ledger nothing else
# would ever honor.
PREMIUM_TYPES = [
    (8, "Premium (ogólne, VIP)"), (1, "VIP Gold"), (0, "VIP Silver"),
    (2, "Magazyn Premium (Safebox)"), (3, "Auto-loot"), (4, "Umysł Rybaka (Fish Mind)"),
    (5, "Szybkie zaręczyny"), (6, "Bonus dropu Yang"), (7, "Rozszerzony sklep"),
]
PREMIUM_COLUMNS = {
    0: "silver_expire", 1: "gold_expire", 2: "safebox_expire", 3: "autoloot_expire",
    4: "fish_mind_expire", 5: "marriage_fast_expire", 6: "money_drop_rate_expire",
    7: "shop_expire", 8: "premium_expire",
}


def ensure_admin_tables():
    """Lazy-created, admin-only bookkeeping. Not something collected on a
    cycle, so it does not belong in collector.py::init() -- created here on
    first use instead, same CREATE TABLE IF NOT EXISTS idiom."""
    rows("""CREATE TABLE IF NOT EXISTS player.web_seban_deleted_players (
      id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
      player_id INT UNSIGNED NOT NULL, player_name VARCHAR(32) NOT NULL,
      snapshot_json LONGTEXT NOT NULL, deleted_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
      KEY(player_id)) ENGINE=InnoDB""")


@app.route("/player/<int:pid>/action/vip", methods=["POST"])
@login_required
def player_action_vip(pid):
    character = one("SELECT id,name,account_id FROM player.player WHERE id=%s", (pid,))
    if not character:
        abort(404)
    try:
        premium_type = int(request.form.get("premium_type", 8))
        days = int(request.form.get("days", 0))
    except ValueError:
        flash("Nieprawidłowe dane formularza.", "error")
        return redirect(url_for("player", pid=pid))
    if premium_type not in PREMIUM_COLUMNS or not 1 <= days <= 3650:
        flash("Liczba dni musi być w zakresie 1-3650.", "error")
        return redirect(url_for("player", pid=pid))
    column = PREMIUM_COLUMNS[premium_type]
    hours = days * 24
    # Same additive-extend logic as CItemShopManager::AddVIP: a still-active
    # grant is extended from its current expiry, not from now, so this
    # behaves exactly like the character buying it again in-game.
    rows(f"""UPDATE account.account SET {column} = CASE
        WHEN {column} <= NOW() THEN DATE_ADD(NOW(), INTERVAL %s HOUR)
        ELSE DATE_ADD({column}, INTERVAL %s HOUR) END WHERE id=%s""",
        (hours, hours, character["account_id"]))
    label = dict(PREMIUM_TYPES).get(premium_type, column)
    rows("INSERT INTO log.log (type,time,who,how,hint) VALUES ('CHARACTER',NOW(),%s,'PANEL_VIP_GRANT',%s)",
         (pid, f"{label} +{days}d"))
    flash(f"Nadano {label} (+{days} dni) dla {character['name']}.")
    return redirect(url_for("player", pid=pid))


@app.route("/manage/bulk-vip", methods=["POST"])
@login_required
def manage_bulk_vip():
    """Same additive-extend grant as player_action_vip, applied in one UPDATE
    to every playerbot_* account instead of one at a time through the UI --
    asked for after a fresh reseed left ~2500 bots with no VIP at all."""
    try:
        premium_type = int(request.form.get("premium_type", 8))
        days = int(request.form.get("days", 0))
    except ValueError:
        flash("Nieprawidłowe dane formularza.", "error")
        return redirect(url_for("manage"))
    if premium_type not in PREMIUM_COLUMNS or not 1 <= days <= 3650:
        flash("Liczba dni musi być w zakresie 1-3650.", "error")
        return redirect(url_for("manage"))
    column = PREMIUM_COLUMNS[premium_type]
    hours = days * 24
    # rows()/fetchall() would come back empty for an UPDATE -- cur.rowcount
    # is the only way to report how many accounts this actually touched.
    with db() as con:
        with con.cursor() as cur:
            cur.execute(f"""UPDATE account.account SET {column} = CASE
                WHEN {column} <= NOW() THEN DATE_ADD(NOW(), INTERVAL %s HOUR)
                ELSE DATE_ADD({column}, INTERVAL %s HOUR) END
                WHERE login LIKE 'playerbot\\_%%'""", (hours, hours))
            affected = cur.rowcount
    label = dict(PREMIUM_TYPES).get(premium_type, column)
    rows("INSERT INTO log.log (type,time,who,how,hint) VALUES ('SYSTEM',NOW(),0,'PANEL_VIP_GRANT_BULK',%s)",
         (f"{label} +{days}d, all playerbot accounts",))
    flash(f"Nadano {label} (+{days} dni) wszystkim botom ({affected} kont).")
    return redirect(url_for("manage"))


@app.route("/player/<int:pid>/action/coins", methods=["POST"])
@login_required
def player_action_coins(pid):
    character = one("SELECT id,name,account_id FROM player.player WHERE id=%s", (pid,))
    if not character:
        abort(404)
    try:
        amount = int(request.form.get("amount", 0))
    except ValueError:
        flash("Nieprawidłowa liczba.", "error")
        return redirect(url_for("player", pid=pid))
    if not 1 <= amount <= 1_000_000:
        flash("Liczba Smoczych Monet musi być w zakresie 1-1 000 000.", "error")
        return redirect(url_for("player", pid=pid))
    rows("UPDATE account.account SET cash = cash + %s WHERE id=%s", (amount, character["account_id"]))
    rows("INSERT INTO log.log (type,time,who,how,hint) VALUES ('CHARACTER',NOW(),%s,'PANEL_DRAGON_COINS',%s)",
         (pid, f"+{amount}"))
    flash(f"Dodano {amount} Smoczych Monet (account.cash) dla {character['name']}.")
    return redirect(url_for("player", pid=pid))


@app.route("/player/<int:pid>/action/rename", methods=["POST"])
@login_required
def player_action_rename(pid):
    character = one("SELECT id,name FROM player.player WHERE id=%s", (pid,))
    if not character:
        abort(404)
    new_name = request.form.get("new_name", "").strip()
    if not (2 <= len(new_name) <= 24) or not new_name.isalnum():
        flash("Nick musi mieć 2-24 znaki alfanumeryczne.", "error")
        return redirect(url_for("player", pid=pid))
    if one("SELECT id FROM player.player WHERE name=%s LIMIT 1", (new_name,)):
        flash(f"Nick '{new_name}' jest już zajęty.", "error")
        return redirect(url_for("player", pid=pid))
    rows("UPDATE player.player SET name=%s WHERE id=%s", (new_name, pid))
    # No PAUSE/STOP command exists in web_admin_queue's live command set
    # (checked web_admin.quest's cmd branches: ITEM/GOLD/LEVEL/WARP/SPEED/
    # RIDER_*/BULK_* only) to safely quiesce a live bot first, so this is a
    # plain write with an honest warning rather than a half-built pause hook.
    flash(f"Zmieniono nick '{character['name']}' → '{new_name}'. Silnik nie zapisuje nazwy z pamięci "
          f"przy CHARACTER::Save, więc żywa postać/bot NIE powinien cofnąć tej zmiany -- ale jeśli mimo "
          f"to wróci stara nazwa, krótko zrestartuj kanał gry, na którym stoi ta postać.")
    return redirect(url_for("player", pid=pid))


@app.route("/player/<int:pid>/action/reset-position", methods=["POST"])
@login_required
def player_action_reset_position(pid):
    character = one("SELECT p.id,p.name," + EMPIRE_EXPR + " AS empire FROM player.player p "
                     "LEFT JOIN account.account a ON a.id=p.account_id "
                     "LEFT JOIN player.player_index pi ON pi.id=p.account_id WHERE p.id=%s", (pid,))
    if not character:
        abort(404)
    empire = int(character.get("empire") or 0)
    if empire not in GM_EMPIRE_STARTS:
        flash("Nie udało się ustalić królestwa tej postaci -- pozycja nie została zmieniona.", "error")
        return redirect(url_for("player", pid=pid))
    x, y, map_index = GM_EMPIRE_STARTS[empire]
    rows("UPDATE player.player SET x=%s,y=%s,map_index=%s WHERE id=%s", (x, y, map_index, pid))
    flash(f"Pozycja postaci {character['name']} zresetowana do stolicy {empire_info(empire)['name']}. "
          f"Jeśli to aktywna postać/bot, silnik może to nadpisać przy najbliższym zapisie z pamięci.")
    return redirect(url_for("player", pid=pid))


@app.route("/player/<int:pid>/action/delete", methods=["POST"])
@login_required
def player_action_delete(pid):
    character = one("SELECT * FROM player.player WHERE id=%s", (pid,))
    if not character:
        abort(404)
    confirm_name = request.form.get("confirm_name", "").strip()
    if confirm_name != character["name"]:
        flash("Wpisana nazwa nie zgadza się z nazwą postaci -- nic nie usunięto.", "error")
        return redirect(url_for("player", pid=pid))
    ensure_admin_tables()
    rows("INSERT INTO player.web_seban_deleted_players (player_id,player_name,snapshot_json) VALUES (%s,%s,%s)",
         (pid, character["name"], json.dumps(character, default=str)))
    rows("DELETE FROM player.item WHERE owner_id=%s", (pid,))
    rows("DELETE FROM player.ikashop_offlineshop WHERE owner=%s", (pid,))
    rows("DELETE FROM player.myshop_pricelist WHERE owner_id=%s", (pid,))
    rows("UPDATE player.player_index SET pid1=IF(pid1=%s,0,pid1), pid2=IF(pid2=%s,0,pid2), "
         "pid3=IF(pid3=%s,0,pid3), pid4=IF(pid4=%s,0,pid4), pid5=IF(pid5=%s,0,pid5) WHERE id=%s",
         (pid, pid, pid, pid, pid, character["account_id"]))
    rows("DELETE FROM player.player WHERE id=%s", (pid,))
    flash(f"Postać '{character['name']}' usunięta. Kopia wiersza w web_seban_deleted_players (id postaci {pid}).")
    return redirect(url_for("players"))


@app.route("/economy")
@login_required
def economy():
    query = request.args.get("q", "").strip().lower()
    latest = one("SELECT MAX(captured_at) AS captured_at FROM player.web_seban_item_snapshot").get("captured_at")
    items = []
    if latest:
        items = rows("""
          SELECT s.vnum, s.socket0, s.amount, COALESCE(p.locale_name, CONCAT('VNUM ', s.vnum)) AS item_name
          FROM player.web_seban_item_snapshot s LEFT JOIN player.item_proto p ON p.vnum=s.vnum
          WHERE s.captured_at=%s ORDER BY s.amount DESC
        """, (latest,))
        for item in items:
            item["item_name"] = resolve_item_display_name(item["vnum"], item["socket0"], game_text(item["item_name"]))
        if query:
            items = [item for item in items if query in item["item_name"].lower() or query == str(item["vnum"])]
    trend = rows("""
      SELECT DATE_FORMAT(captured_at, '%%m-%%d %%H:%%i') AS captured_at, value FROM player.web_seban_metric_snapshot
      WHERE metric='total_yang' AND captured_at >= NOW() - INTERVAL 7 DAY ORDER BY captured_at
    """)
    return render_template("economy.html", latest=latest, items=items[:500], query=query, trend=trend)


@app.route("/economy/item/<int:vnum>")
@login_required
def economy_item(vnum):
    item = one("SELECT vnum,COALESCE(locale_name,CONCAT('VNUM ',vnum)) AS item_name FROM player.item_proto WHERE vnum=%s", (vnum,)) or {"vnum": vnum, "item_name": f"VNUM {vnum}"}
    item["item_name"] = game_text(item["item_name"])
    history = rows("""SELECT DATE_FORMAT(captured_at, '%%m-%%d %%H:%%i') AS captured_at,amount
      FROM player.web_seban_item_snapshot WHERE vnum=%s AND captured_at >= NOW() - INTERVAL 14 DAY ORDER BY captured_at""", (vnum,))
    return render_template("economy_item.html", item=item, history=history)


def _shop_trend(current, previous):
    if previous is None or current == previous:
        return "flat"
    return "up" if current > previous else "down"


def shop_item_market_row(vnum, socket0, shop_latest):
    """Current (or last-known) shop stats for one (vnum, socket0) pair, with a
    trend against the earliest snapshot within the last 24h -- degrades to
    'flat'/no baseline while history is still short, rather than guessing.
    Resolves its own display name (instead of taking one from the caller) so
    that the same vnum -- e.g. 50300, the generic Skill Book -- can come back
    with a different name per socket0."""
    proto = one("SELECT COALESCE(locale_name, CONCAT('VNUM ', vnum)) AS item_name FROM player.item_proto WHERE vnum=%s", (vnum,))
    base_name = game_text(proto["item_name"]) if proto else f"VNUM {vnum}"
    item_name = resolve_item_display_name(vnum, socket0, base_name)
    now_row = one("""SELECT captured_at, offers, total_units, total_value
      FROM player.web_seban_shop_item_snapshot WHERE vnum=%s AND socket0=%s ORDER BY captured_at DESC LIMIT 1""", (vnum, socket0))
    if not now_row:
        return {"vnum": vnum, "socket0": socket0, "item_name": item_name, "on_market": False, "offers": 0,
                "total_units": 0, "avg_price": 0, "last_seen": None,
                "units_trend": "flat", "price_trend": "flat"}
    baseline = one("""SELECT total_units, total_value FROM player.web_seban_shop_item_snapshot
      WHERE vnum=%s AND socket0=%s AND captured_at >= NOW() - INTERVAL 24 HOUR ORDER BY captured_at ASC LIMIT 1""", (vnum, socket0))
    units = int(now_row["total_units"])
    value = int(now_row["total_value"])
    prev_units = int(baseline["total_units"]) if baseline else None
    prev_value = int(baseline["total_value"]) if baseline else None
    prev_avg = round(prev_value / prev_units) if baseline and prev_units else None
    return {
        "vnum": vnum, "socket0": socket0, "item_name": item_name, "offers": int(now_row["offers"]),
        "total_units": units, "avg_price": round(value / units) if units else 0,
        "on_market": now_row["captured_at"] == shop_latest, "last_seen": now_row["captured_at"],
        "units_trend": _shop_trend(units, prev_units),
        "price_trend": _shop_trend(round(value / units) if units else 0, prev_avg),
    }


SALE_HINT_RE = re.compile(r"^(\d+)\s+x(\d+)\s+za\s+(\d+)$")


def map_short_code(index):
    """'Shinsoo M1 - Yongan' -> 'M1'. Every map that ever hosts an offline
    shop follows this naming; falls back to the full name for one that does
    not (a dungeon, say), rather than showing nothing."""
    match = re.search(r"M\d+", MAP_NAMES.get(int(index or 0), ""))
    return match.group(0) if match else map_name(index)


def _item_display_name(vnum, socket0=0):
    proto = one("SELECT COALESCE(locale_name, CONCAT('VNUM ', vnum)) AS item_name FROM player.item_proto WHERE vnum=%s", (vnum,))
    base_name = game_text(proto["item_name"]) if proto else f"VNUM {vnum}"
    return resolve_item_display_name(vnum, socket0, base_name)


def shop_sales_velocity(hours=24, limit=15, only_skillbooks=False):
    """Ranks items by how many times bots actually bought them off a stall in
    the last `hours` (log.log how='PLAYERBOT_STALL_SOLD'), not by what is
    merely listed -- that is what shop_item_market_row() already covers.
    Demand signal for 'which price should go up', per operator's ask.
    log.log itself has no socket0 column, but log.what is the sold item's
    own id, and that row often still exists in player.item (confirmed on
    live data: ~79% over 24h, ~90% within the last hour -- it only
    disappears once a bot actually consumes the book). Joining it back lets
    Skill Book sales split by the taught skill; a sale whose item is
    already gone can't be attributed to any specific skill, so it is
    dropped rather than shown as a 'which price should I raise' row for an
    unknown skill -- that gave no real signal (confirmed with operator: the
    lumped generic row was topping the ranking and telling them nothing
    actionable). only_skillbooks=True narrows the whole query to Skill Book
    vnums, for a dedicated 'top skill books' panel."""
    vnum_filter = " AND l.vnum IN ({})".format(",".join(str(v) for v in SKILLBOOK_VNUMS)) if only_skillbooks else ""
    raw = rows(f"""SELECT l.vnum, l.hint, l.time, i.socket0 FROM log.log l
      LEFT JOIN player.item i ON i.id=l.what
      WHERE l.how='PLAYERBOT_STALL_SOLD' AND l.time >= NOW() - INTERVAL %s HOUR{vnum_filter}""", (hours,))
    cutoff = datetime.now() - timedelta(hours=hours / 2)
    agg = {}
    for r in raw:
        match = SALE_HINT_RE.match(game_text(r["hint"]))
        if not match:
            continue
        vnum, qty, price = int(match.group(1)), int(match.group(2)), int(match.group(3))
        is_skillbook = vnum in SKILLBOOK_VNUMS
        socket0 = int(r["socket0"] or 0) if is_skillbook else 0
        if is_skillbook and socket0 == 0:
            continue
        key = (vnum, socket0)
        a = agg.setdefault(key, {"sales": 0, "units": 0, "revenue": 0,
                                   "recent_units": 0, "recent_revenue": 0, "older_units": 0, "older_revenue": 0})
        a["sales"] += 1
        a["units"] += qty
        a["revenue"] += price
        bucket = "recent" if r["time"] >= cutoff else "older"
        a[f"{bucket}_units"] += qty
        a[f"{bucket}_revenue"] += price
    ranked = sorted(agg.items(), key=lambda kv: kv[1]["sales"], reverse=True)[:limit]
    result = []
    for (vnum, socket0), a in ranked:
        recent_avg = round(a["recent_revenue"] / a["recent_units"]) if a["recent_units"] else None
        older_avg = round(a["older_revenue"] / a["older_units"]) if a["older_units"] else None
        result.append({
            "vnum": vnum, "item_name": _item_display_name(vnum, socket0),
            "sales": a["sales"], "units": a["units"],
            "avg_price": round(a["revenue"] / a["units"]) if a["units"] else 0,
            "per_hour": round(a["sales"] / hours, 1),
            "price_trend": _shop_trend(recent_avg, older_avg) if recent_avg is not None else "flat",
        })
    return result


def recent_shop_sales(limit=10):
    """Last N completed stall sales, newest first. The engine's sale log
    (log.log how='PLAYERBOT_STALL_SOLD') only ever records the seller -- no
    buyer identity exists anywhere for an offline-shop purchase, confirmed
    against a live sample -- so this is honestly a 'who sold what' feed, not
    a two-sided trade feed. Joined to player.item on log.what (the sold
    item's own id) to recover socket0 for Skill Books -- see
    shop_sales_velocity() for the match-rate note."""
    raw = rows("""SELECT l.time, l.who, l.x, l.y, l.vnum, l.hint, i.socket0 FROM log.log l
      LEFT JOIN player.item i ON i.id=l.what
      WHERE l.how='PLAYERBOT_STALL_SOLD' ORDER BY l.time DESC LIMIT %s""", (limit,))
    sales = []
    for r in raw:
        match = SALE_HINT_RE.match(game_text(r["hint"]))
        qty = int(match.group(2)) if match else 1
        price = int(match.group(3)) if match else 0
        socket0 = int(r["socket0"] or 0) if int(r["vnum"]) in SKILLBOOK_VNUMS else 0
        seller = one("""SELECT p.name, pi.empire FROM player.player p
          JOIN player.player_index pi ON pi.id=p.account_id WHERE p.id=%s""", (r["who"],))
        map_index = next((idx for idx, b in MAP_BOUNDS.items()
                           if b[0] <= r["x"] < b[0] + b[2] and b[1] <= r["y"] < b[1] + b[3]), None)
        sales.append({
            "time": r["time"].strftime("%H:%M:%S"), "vnum": r["vnum"], "item_name": _item_display_name(r["vnum"], socket0),
            "icon_url": item_icon_url(r["vnum"]),
            "qty": qty, "price": price,
            "seller": (seller or {}).get("name") or f"pid {r['who']}",
            "seller_id": int(r["who"]),
            "empire": int((seller or {}).get("empire") or 0),
            "map_name": map_name(map_index) if map_index is not None else "—",
        })
    return sales


@app.route("/economy/shops")
@login_required
def economy_shops():
    # The collector's first snapshot creates the table; before it this page is
    # empty, like the dashboard's chart, not a 500.
    try:
        latest = one("SELECT MAX(captured_at) AS captured_at FROM player.web_seban_shop_snapshot").get("captured_at")
    except pymysql.MySQLError:
        latest = None
    by_map = []
    empire_totals = {empire: {"shops": 0, "offers": 0, "items": 0, "value": 0} for empire in EMPIRES}
    if latest:
        by_map = rows("""SELECT map_index, empire, shop_count, offer_count, item_count, total_value
          FROM player.web_seban_shop_snapshot WHERE captured_at=%s ORDER BY empire, shop_count DESC""", (latest,))
        for m in by_map:
            m["map_name"] = map_name(m["map_index"])
            m["map_short"] = map_short_code(m["map_index"])
            totals = empire_totals.setdefault(int(m["empire"]), {"shops": 0, "offers": 0, "items": 0, "value": 0})
            totals["shops"] += int(m["shop_count"])
            totals["offers"] += int(m["offer_count"])
            totals["items"] += int(m["item_count"])
            totals["value"] += int(m["total_value"])
    kpi = {
        "shops": sum(t["shops"] for t in empire_totals.values()),
        "offers": sum(t["offers"] for t in empire_totals.values()),
        "items": sum(t["items"] for t in empire_totals.values()),
        "value": sum(t["value"] for t in empire_totals.values()),
        "transactions_total": int(one("SELECT COUNT(*) AS n FROM log.log WHERE how='PLAYERBOT_STALL_SOLD'").get("n") or 0),
        "transactions_24h": int(one("SELECT COUNT(*) AS n FROM log.log WHERE how='PLAYERBOT_STALL_SOLD' AND time >= NOW() - INTERVAL 24 HOUR").get("n") or 0),
    }

    # Same label/series pivot as maps(): one line per empire, values aligned
    # to a shared, appearance-ordered label list, missing points left as gaps
    # (null) rather than false zeros the market never actually hit.
    raw_trend = rows("""SELECT DATE_FORMAT(captured_at, '%%m-%%d %%H:%%i') AS label, empire, SUM(total_value) AS total_value
      FROM player.web_seban_shop_snapshot WHERE captured_at >= NOW() - INTERVAL 7 DAY
      GROUP BY captured_at, empire ORDER BY captured_at ASC""")
    trend_labels, trend_values = [], {empire: {} for empire in EMPIRES}
    for row in raw_trend:
        empire = int(row["empire"] or 0)
        if empire not in trend_values:
            continue
        if row["label"] not in trend_labels:
            trend_labels.append(row["label"])
        trend_values[empire][row["label"]] = int(row["total_value"] or 0)
    value_trend = {"labels": trend_labels, "series": [
        {"id": empire, "name": empire_info(empire)["name"],
         "data": [trend_values[empire].get(label) for label in trend_labels]}
        for empire in EMPIRES
    ]}

    shop_latest = one("SELECT MAX(captured_at) AS captured_at FROM player.web_seban_shop_item_snapshot").get("captured_at")
    query = request.args.get("q", "").strip()
    market_items = []
    if query:
        # A search can name an item with zero active offers right now -- look
        # it up regardless of whether it is in today's top ranking, and
        # shop_item_market_row() reports whether it is on the market or was
        # last seen there, rather than silently returning nothing.
        # A generic term can match dozens/hundreds of item_proto rows (e.g.
        # a common Polish word); items that have ever actually shown up in a
        # shop are what the operator is asking about, so they are ranked
        # first instead of getting cut off by LIMIT in plain vnum order.
        candidates = rows("""SELECT ip.vnum, COALESCE(ip.locale_name, CONCAT('VNUM ', ip.vnum)) AS item_name
          FROM player.item_proto ip
          LEFT JOIN (SELECT vnum, MAX(captured_at) AS seen FROM player.web_seban_shop_item_snapshot GROUP BY vnum) h
            ON h.vnum = ip.vnum
          WHERE ip.vnum=%s OR ip.locale_name LIKE %s
          ORDER BY (h.seen IS NOT NULL) DESC, ip.vnum LIMIT 20""",
          (int(query) if query.isdigit() else -1, f"%{query}%"))
        # (vnum, socket0) pairs to look up. A plain item_proto match on the
        # generic Skill Book (50300) is expanded into every specific skill
        # variant this shop history has ever seen; a query is also matched
        # against skill names directly, since item_proto's own locale_name
        # for 50300 is always "Ksiega Umiejetnosci" and can never mention
        # e.g. "Berserk" the way resolve_item_display_name()'s output does.
        keys = []
        for c in candidates:
            if int(c["vnum"]) in SKILLBOOK_VNUMS:
                variants = rows("SELECT DISTINCT socket0 FROM player.web_seban_shop_item_snapshot WHERE vnum=%s", (c["vnum"],))
                keys += [(int(c["vnum"]), int(v["socket0"])) for v in variants]
            else:
                keys.append((int(c["vnum"]), 0))
        query_lower = query.lower()
        for skill_vnum, skill_name in SKILL_NAMES.items():
            if query_lower in skill_name.lower():
                for book_vnum in SKILLBOOK_VNUMS:
                    keys.append((book_vnum, skill_vnum))
        seen_keys = set()
        keys = [k for k in keys if not (k in seen_keys or seen_keys.add(k))][:20]
        market_items = [shop_item_market_row(vnum, socket0, shop_latest) for vnum, socket0 in keys]
    elif shop_latest:
        top_rows = rows("""SELECT s.vnum, s.socket0
          FROM player.web_seban_shop_item_snapshot s
          WHERE s.captured_at=%s ORDER BY s.total_units DESC LIMIT 15""", (shop_latest,))
        market_items = [shop_item_market_row(r["vnum"], r["socket0"], shop_latest) for r in top_rows]

    return render_template("economy_shops.html", latest=latest, by_map=by_map, query=query,
                            empire_totals=empire_totals, kpi=kpi, value_trend=value_trend, market_items=market_items,
                            sales_velocity=shop_sales_velocity(), skillbook_velocity=shop_sales_velocity(limit=5, only_skillbooks=True),
                            recent_sales=recent_shop_sales(10))


@app.route("/api/shop-feed")
@login_required
def api_shop_feed():
    return {"ok": True, "sales": recent_shop_sales(10)}


@app.route("/items")
@login_required
def items_database():
    query = request.args.get("q", "").strip()
    item_type = request.args.get("type", "").strip()
    where, params = [], []
    if query:
        where.append("(p.vnum=%s OR p.locale_name LIKE %s OR p.name LIKE %s)")
        params += [int(query) if query.isdigit() else -1, f"%{query}%", f"%{query}%"]
    if item_type.isdigit():
        where.append("p.type=%s")
        params.append(int(item_type))
    predicate = " WHERE " + " AND ".join(where) if where else ""
    total = one("SELECT COUNT(*) AS count FROM player.item_proto p" + predicate, params).get("count", 0)
    records = rows("SELECT p.vnum,p.name,p.locale_name,p.type,p.subtype,p.size,p.gold,p.shop_buy_price FROM player.item_proto p" + predicate + " ORDER BY p.vnum", params)
    for item in records:
        item["name"] = game_text(item.get("locale_name") or item.get("name"))
    types = rows("SELECT type,COUNT(*) AS count,MIN(vnum) AS icon_vnum FROM player.item_proto GROUP BY type ORDER BY type")
    for category in types:
        index = int(category["type"])
        category["label"] = ITEM_TYPE_NAMES[index] if 0 <= index < len(ITEM_TYPE_NAMES) else f"ITEM_TYPE_{index}"
    return render_template("items.html", items=records, types=types, selected_type=item_type, query=query, total=total)


@app.route("/gm-commands")
@login_required
def gm_commands():
    return render_template("gm_commands.html", commands=GM_COMMANDS)


@app.route("/accounts", methods=["GET", "POST"])
@login_required
def accounts():
    authorities = ("PLAYER", "LOW_WIZARD", "GOD", "HIGH_WIZARD", "IMPLEMENTOR")
    if request.method == "POST":
        login = request.form.get("login", "").strip()
        password = request.form.get("password", "")
        email = request.form.get("email", "").strip()[:120]
        empire = max(1, min(3, int(request.form.get("empire", 1) or 1)))
        authority = request.form.get("authority", "PLAYER")
        gm_name = request.form.get("gm_name", "").strip()
        deletion_code = request.form.get("deletion_code", "").strip()
        try:
            gm_job = int(request.form.get("gm_job", 0) or 0)
        except ValueError:
            gm_job = -1
        gm_gender = request.form.get("gm_gender", "classic")
        # account.login is varchar(16) on mt2009 and varchar(30) on r40250; a
        # longer one is "Data too long" from the database, not a form error.
        login_max = 16 if ENGINE_MT2009 else 30
        if not (3 <= len(login) <= login_max and login.replace("_", "").isalnum() and len(password) >= 6 and authority in authorities):
            flash(f"Login ma mieć 3–{login_max} znaków (litery, cyfry, _), a hasło minimum 6 znaków.", "error")
        elif not (deletion_code.isdigit() and len(deletion_code) == 7):
            flash("Kod usunięcia postaci ma zawierać dokładnie 7 cyfr.", "error")
        elif authority != "PLAYER" and not re.fullmatch(GM_NAME_PATTERN, gm_name):
            flash("Nick postaci GM ma mieć 2–24 znaki. Dozwolony jest też jeden prefiks, np. [GM]Seban lub [GA]Seban.", "error")
        elif authority != "PLAYER" and gm_job not in dict(GM_JOB_OPTIONS):
            flash("Wybierz poprawną klasę postaci GM.", "error")
        elif authority != "PLAYER" and gm_gender not in dict(GM_GENDER_OPTIONS):
            flash("Wybierz prawidłową płeć postaci GM.", "error")
        else:
            account_id = None
            player_id = None
            try:
                with db() as con:
                    with con.cursor() as cur:
                        if authority != "PLAYER":
                            cur.execute("SELECT id FROM player.player WHERE name=%s LIMIT 1", (gm_name,))
                            if cur.fetchone():
                                raise ValueError("Taki nick postaci już istnieje.")
                        con.begin()
                        # The mt2009 account table has no empire column (the kingdom
                        # lives in player_index, written below for a GM character and
                        # by the game itself for a player's first character); naming
                        # it refused every account on the 2.x line ("Unknown column
                        # 'empire' in 'INSERT INTO'", NieBijOddam, 11 September).
                        if ENGINE_MT2009:
                            cur.execute("INSERT INTO account.account (login,password,social_id,email,status) VALUES (%s,PASSWORD(%s),%s,%s,'OK')", (login, password, deletion_code, email))
                        else:
                            cur.execute("INSERT INTO account.account (login,password,social_id,email,status,empire) VALUES (%s,PASSWORD(%s),%s,%s,'OK',%s)", (login, password, deletion_code, email, empire if authority != "PLAYER" else 0))
                        if authority != "PLAYER":
                            account_id = cur.lastrowid
                            x, y, map_index = GM_EMPIRE_STARTS[empire]
                            st, ht, dx, iq, hp, mp = GM_JOB_STARTS[gm_job]
                            character_race = GM_RACE_BY_CLASS_GENDER[(gm_job, gm_gender)]
                            cur.execute("""INSERT INTO player.player
                              (account_id,name,job,dir,x,y,map_index,exit_x,exit_y,exit_map_index,hp,mp,stamina,random_hp,random_sp,level,st,ht,dx,iq,stat_point,skill_point,sub_skill_point,part_main,part_base,part_hair,skill_group,horse_hp,horse_stamina,horse_level,horse_hp_droptime,horse_riding,horse_skill_point""" + ("" if ENGINE_MT2009 else ",bank_value") + """)
                              VALUES (%s,%s,%s,0,%s,%s,%s,%s,%s,%s,%s,%s,1000,0,0,1,%s,%s,%s,%s,0,0,0,0,0,0,0,0,0,0,0,0,0""" + ("" if ENGINE_MT2009 else ",0") + """)""",
                              (account_id, gm_name, character_race, x, y, map_index, x, y, map_index, hp, mp, st, ht, dx, iq))
                            player_id = cur.lastrowid
                            # Metin reads character slots from player_index.  A player row
                            # without this entry exists in SQL but is invisible at login.
                            cur.execute("""INSERT INTO player.player_index (id,pid1,pid2,pid3,pid4,empire)
                              VALUES (%s,%s,0,0,0,%s)
                              ON DUPLICATE KEY UPDATE pid1=VALUES(pid1),pid2=0,pid3=0,pid4=0,empire=VALUES(empire)""",
                              (account_id, player_id, empire))
                            cur.execute("INSERT INTO common.gmlist (mAccount,mName,mContactIP,mServerIP,mAuthority) VALUES (%s,%s,'','ALL',%s)", (login, gm_name, authority))
                        con.commit()
                if authority != "PLAYER":
                    flash(f"Utworzono konto i postać GM „{gm_name}”. Postać jest dostępna od razu; uprawnienia GM staną się aktywne po restarcie usług gry.")
                else:
                    flash("Konto utworzone.")
                return redirect(url_for("accounts"))
            except (pymysql.MySQLError, ValueError) as exc:
                try: con.rollback()
                except Exception: pass
                # The original Metin tables use MyISAM, so a failed multi-table
                # creation is not rolled back by MariaDB.  Remove only records
                # made by this request so an empty account is never left behind.
                if account_id:
                    try:
                        with db() as cleanup_con:
                            with cleanup_con.cursor() as cleanup:
                                cleanup.execute("DELETE FROM common.gmlist WHERE mAccount=%s AND mName=%s", (login, gm_name))
                                if player_id:
                                    cleanup.execute("DELETE FROM player.player WHERE id=%s AND account_id=%s", (player_id, account_id))
                                cleanup.execute("DELETE FROM player.player_index WHERE id=%s", (account_id,))
                                cleanup.execute("DELETE FROM account.account WHERE id=%s AND login=%s", (account_id, login))
                    except pymysql.MySQLError:
                        pass
                flash(f"Nie utworzono konta: {exc.args[1] if isinstance(exc, pymysql.MySQLError) and len(exc.args)>1 else exc}", "error")
    account_query = request.args.get("q", "").strip()[:60]
    display = request.args.get("display", "100")
    if display not in ("100", "1000", "all"):
        display = "100"
    where, params = [], []
    if account_query:
        where.append("(a.login LIKE %s OR EXISTS (SELECT 1 FROM player.player p WHERE p.account_id=a.id AND p.name LIKE %s))")
        params.extend([f"%{account_query}%", f"%{account_query}%"])
    query_sql = "SELECT a.id,a.login,a.email," + ("0 AS empire" if ENGINE_MT2009 else "a.empire") + ",a.create_time,a.last_play FROM account.account a"
    if where:
        query_sql += " WHERE " + " AND ".join(where)
    query_sql += " ORDER BY a.id DESC"
    if display != "all":
        query_sql += " LIMIT %s"
        params.append(int(display))
    recent = rows(query_sql, params)
    return render_template("accounts.html", accounts=recent, authorities=authorities, jobs=GM_JOB_OPTIONS, genders=GM_GENDER_OPTIONS, account_query=account_query, display=display)


@app.route("/maps")
@login_required
def maps():
    raw = rows("""
      SELECT DATE_FORMAT(captured_at, '%%m-%%d %%H:%%i') AS label, map_index, character_count
      FROM player.web_seban_map_snapshot
      WHERE captured_at >= NOW() - INTERVAL 24 HOUR ORDER BY captured_at ASC
    """)
    labels, series = [], {index: {} for index, _name in TRACKED_MAP_OPTIONS}
    for row in raw:
        index = int(row["map_index"] or 0)
        if index not in series:
            continue
        if row["label"] not in labels:
            labels.append(row["label"])
        series[index][row["label"]] = int(row["character_count"] or 0)
    chart = {"labels": labels, "series": [
        {"id": index, "name": name, "data": [values.get(label, 0) for label in labels]}
        for index, name in TRACKED_MAP_OPTIONS for values in (series[index],)
    ]}
    current = {int(row["map_index"]): row["character_count"] for row in live_map_counts()}
    latest = [{"map_index": index, "character_count": current.get(index, 0)} for index, _name in TRACKED_MAP_OPTIONS]
    return render_template("maps.html", chart=chart, latest=latest)


@app.route("/changelog")
@login_required
def changelog():
    return render_template("changelog.html", entries=changelog_entries(), panel_version=PANEL_VERSION)


@app.route("/accounts/<int:aid>/characters")
@login_required
def account_characters(aid):
    characters = rows("""SELECT id,name,level,job,map_index FROM player.player
      WHERE account_id=%s ORDER BY level DESC""", (aid,))
    for character in characters:
        character["job_name"] = class_profile(character["job"])["name"]
        character["map_name"] = map_name(character["map_index"])
    return {"ok": True, "characters": characters}


@app.route("/api/live-bots")
@login_required
def api_live_bots():
    global_top = one("SELECT id FROM player.player WHERE " + BOT_IS_BARE + " ORDER BY level DESC,exp DESC LIMIT 1")
    return {"ok": True, "updated_at": int(datetime.now().timestamp() * 1000), "maps": MAP_NAMES, "bounds": MAP_BOUNDS, "global_top_id": global_top.get("id"), "bots": live_bots()}


@app.route("/api/news-feed")
@login_required
def api_news_feed():
    return {"ok": True, "events": news_feed_events()}


@app.route("/system")
@login_required
def system():
    samples = rows("""
      SELECT DATE_FORMAT(captured_at, '%%H:%%i') AS label, cpu_percent, ram_percent, ram_used_mb, ram_total_mb,
             disk_percent, disk_used_mb, disk_total_mb
      FROM player.web_seban_system_snapshot WHERE captured_at >= NOW() - INTERVAL 24 HOUR ORDER BY captured_at
    """)
    current = samples[-1] if samples else {}
    return render_template("system.html", samples=samples, current=current)


@app.route("/api/system-current")
@login_required
def api_system_current():
    return {"ok": True, "system": one("SELECT * FROM player.web_seban_system_snapshot ORDER BY captured_at DESC LIMIT 1")}


@app.route("/rankings")
@login_required
def rankings():
    kinds = {
        "level": "Poziom", "armor": "Zbroja", "weapon": "Broń", "weapon30": "Broń 30 Lv",
        # Bez "Polowanie": na tej linii silnika levelup.quest lezy w
        # quest/_unused, zaden hook zabicia nie strzela i licznik stoi na zero
        # dla kazdego bota - ranking miał wiec 100 pozycji z "Ukonczone do Lv 0"
        # (Tieru, 13 wrzesnia).
        "gold": "Yang", "items": "Przedmioty", "horse": "Koń", "biologist": "Biolog",
        "shops": "Otwarte stragany", "skills": "Umiejętności", "plus9": "Przedmiot +9", "playtime": "Czas gry", "bosses": "Bossy", "refine": "Pomyślne ulepszenia", "refine_rate": "Skuteczność ulepszeń",
    }
    kind = request.args.get("type", "level")
    if kind not in kinds:
        kind = "level"
    weapon30_sort = request.args.get("sort", "avg") if kind == "weapon30" else "avg"
    if weapon30_sort not in ("avg", "skill", "upgrade"):
        weapon30_sort = "avg"
    ranking = bot_ranking(kind, weapon30_sort)
    ids = [row["id"] for row in ranking]
    if ids:
        # Which kingdom each of them belongs to. player_index.empire, because
        # that is the column the core reads when it decides where a bot lives;
        # the account's own copy was left at Chunjo for the whole cohort.
        marks = ",".join(["%s"] * len(ids))
        empire_rows = rows(
            "SELECT p.id, " + EMPIRE_EXPR + " AS empire"
            " FROM player.player p"
            " LEFT JOIN player.player_index pi ON pi.id=p.account_id"
            " LEFT JOIN account.account a ON a.id=p.account_id"
            " WHERE p.id IN (" + marks + ")", ids)
        empires = {row["id"]: row["empire"] for row in empire_rows}
        for row in ranking:
            row["empire"] = empires.get(row["id"], 0)
        progress_rows = rows("SELECT id,level,exp,job FROM player.player WHERE id IN (" + ",".join(["%s"] * len(ids)) + ")", ids)
        progress = {row["id"]: experience_progress(row["level"], row["exp"]) for row in progress_rows}
        jobs = {row["id"]: row["job"] for row in progress_rows}
        for row in ranking:
            row["job"] = jobs.get(row["id"], 0)
            row["experience"] = progress.get(row["id"], {"percent": 0})
    for row in ranking:
        if kind == "weapon30":
            row["detail"] = "Średnie obrażenia: %s%% · Obrażenia umiejętności: %s%% · %s" % (
                int(row.get("avg_damage") or 0), int(row.get("skill_damage") or 0), game_text(row.get("item_name")))
        else:
            row["detail"] = game_text(row.get("detail"))
    return render_template("rankings.html", kinds=kinds, kind=kind, ranking=ranking, weapon30_sort=weapon30_sort)


@app.route("/season")
def season():
    """Weekly season from the three indexed event types only."""
    if time.time() - _season_cache["at"] < 600:
        return render_template("season.html", weekly=_season_cache["weekly"], records=_season_cache["records"])
    weekly = rows("""SELECT p.id,p.name,p.level,
        SUM(l.how='STONE_KILL') AS metins,
        SUM(l.how='BOSS_KILL') AS bosses,
        SUM(l.how='REFINE SUCCESS' AND (l.hint LIKE '%%+7' OR l.hint LIKE '%%+8' OR l.hint LIKE '%%+9')) AS refine7
        FROM log.log l JOIN player.player p ON p.id=l.who
        WHERE l.time>=NOW()-INTERVAL 7 DAY AND """ + ranking_scope_sql("p") + """
          AND l.how IN ('STONE_KILL','BOSS_KILL','REFINE SUCCESS')
        GROUP BY p.id ORDER BY (SUM(l.how='STONE_KILL')*150+SUM(l.how='BOSS_KILL')*500+SUM(l.how='REFINE SUCCESS' AND (l.hint LIKE '%%+7' OR l.hint LIKE '%%+8' OR l.hint LIKE '%%+9'))*200) DESC,p.level DESC LIMIT 30""")
    for row in weekly:
        row["points"] = int(row.get("metins") or 0)*150 + int(row.get("bosses") or 0)*500 + int(row.get("refine7") or 0)*200
    records = one("""SELECT
        SUM(l.how='STONE_KILL') AS metins,
        SUM(l.how='BOSS_KILL') AS bosses,
        SUM(l.how='REFINE SUCCESS' AND (l.hint LIKE '%%+7' OR l.hint LIKE '%%+8' OR l.hint LIKE '%%+9')) AS refine7,
        (SELECT MAX(level) FROM player.player) AS level
        FROM log.log l
        WHERE l.time>=NOW()-INTERVAL 7 DAY
          AND l.how IN ('STONE_KILL','BOSS_KILL','REFINE SUCCESS')""")
    _season_cache.update(at=time.time(), weekly=weekly, records=records)
    return render_template("season.html", weekly=weekly, records=records)





@app.route("/economy/itemshop")
@login_required
def economy_itemshop():
    totals = one("""SELECT COALESCE(SUM(cash),0) cash,COALESCE(SUM(cash_mark),0) mileage,
                       SUM(cash>0) cash_accounts,SUM(cash_mark>0) mileage_accounts
                    FROM account.account WHERE status IN ('OK','BLOCK')""") or {}
    top_cash = rows("""SELECT id,login,cash,cash_mark AS mileage FROM account.account
                       WHERE status IN ('OK','BLOCK') AND cash>0 ORDER BY cash DESC,id ASC LIMIT 15""")
    top_mileage = rows("""SELECT id,login,cash,cash_mark AS mileage FROM account.account
                          WHERE status IN ('OK','BLOCK') AND cash_mark>0 ORDER BY cash_mark DESC,id ASC LIMIT 10""")
    purchases = rows("""SELECT COALESCE(p.name,CONCAT('PID ',l.pid)) buyer_name,
                        COALESCE(ip.locale_name,CONCAT('VNUM ',l.vnum)) item_name,
                        l.time date_of_buy,l.vnum vnum_icon
                        FROM log.itemshop l
                        LEFT JOIN player.player p ON p.id=l.pid
                        LEFT JOIN player.item_proto ip ON ip.vnum=l.vnum
                        ORDER BY l.time DESC LIMIT 80""")
    popular = rows("""SELECT l.vnum,COALESCE(ip.locale_name,CONCAT('VNUM ',l.vnum)) item_name,
                      COUNT(*) total FROM log.itemshop l
                      LEFT JOIN player.item_proto ip ON ip.vnum=l.vnum
                      GROUP BY l.vnum,ip.locale_name ORDER BY total DESC,item_name LIMIT 10""")
    daily = rows("""SELECT DATE_FORMAT(time,'%%d.%%m') label,COUNT(*) total
                    FROM log.itemshop WHERE time>=NOW()-INTERVAL 14 DAY
                    GROUP BY DATE(time) ORDER BY DATE(time)""")
    for row in purchases + popular:
        row["item_name"] = game_text(row.get("item_name"))
    return render_template("economy_itemshop.html", totals=totals, top_cash=top_cash,
                           top_mileage=top_mileage, purchases=purchases, popular=popular,
                           daily=daily)

@app.route("/diagnostics")
@login_required
def diagnostics():
    return render_template("diagnostics.html", diagnostic=fishing_diagnostics())

@app.route("/events", methods=["GET", "POST"])
@login_required
def events():
    rows, nows = read_events()
    if request.method == "POST":
        action = request.form.get("action", "")
        if action == "save":
            new_rows = []
            for index in range(16):
                kind = request.form.get(f"r{index}_kind")
                if kind is None:
                    break
                if kind not in EVENT_KINDS or request.form.get(f"r{index}_delete"):
                    continue
                start = event_hhmm(request.form.get(f"r{index}_start"))
                end = event_hhmm(request.form.get(f"r{index}_end"))
                try:
                    value = max(0, min(1000, int(request.form.get(f"r{index}_value") or 0)))
                except ValueError:
                    value = -1
                if not start or not end or start == "24:00" or value < 0:
                    flash(f"Wiersz {index + 1}: podaj poprawne godziny i wartość 0–1000%.", "error")
                    return redirect(url_for("events"))
                days = [day for day in range(1, 8) if request.form.get(f"r{index}_d{day}")]
                new_rows.append({"kind": kind, "days": days, "start": start, "end": end,
                                 "value": 0 if kind == "chest" else value,
                                 "on": bool(request.form.get(f"r{index}_on"))})
            try:
                write_events(new_rows, nows)
            except OSError:
                flash("Nie udało się zapisać harmonogramu eventów.", "error")
            else:
                flash("Harmonogram zapisany. Rdzeń zastosuje go w ciągu pięciu sekund.", "success")
            return redirect(url_for("events"))
        kind = request.form.get("kind", "")
        if kind not in EVENT_KINDS:
            return redirect(url_for("events"))
        if action == "now":
            try:
                minutes = max(5, min(1440, int(request.form.get("minutes") or 60)))
                value = max(1, min(1000, int(request.form.get("value") or 50)))
            except ValueError:
                minutes, value = 60, 50
            nows[kind] = {"until": int(time.time()) + minutes * 60, "value": 0 if kind == "chest" else value}
            write_events(rows, nows)
            flash(f"Event aktywowany na {minutes} min. Rdzeń odczyta go w ciągu pięciu sekund.", "success")
        elif action == "stop":
            nows.pop(kind, None)
            write_events(rows, nows)
            flash("Natychmiastowy event został zatrzymany.", "success")
        return redirect(url_for("events"))
    shown = list(rows) + [{"kind": "", "days": list(range(1, 8)), "start": "20:00", "end": "21:00", "value": 50, "on": True} for _ in range(max(0, 4 - len(rows)))]
    return render_template("events.html", rows=shown, nows=nows, status=read_events_status(),
                           event_kinds=EVENT_KINDS, event_labels=EVENT_LABELS,
                           day_names=EVENT_DAY_NAMES, now_minutes=EVENT_NOW_MINUTES,
                           now_epoch=int(time.time()))

@app.route("/manage")
@login_required
def manage():
    map_counts = live_map_counts()
    current_settings = settings()
    # Ile botow na ktorym kanale - z drugim kanalem wlaczonym sama suma nie
    # mowi, czy podzial wyszedl ("warto by dodac statystyke ile jest botow na
    # CH1 a ile na CH2", hunmar, 19 wrzesnia). Bez drugiego kanalu wszystko
    # jest na pierwszym i rozbicie sie nie pokazuje.
    bots = live_bots()
    per_channel = {}
    for bot in bots:
        per_channel[int(bot.get("channel") or 1)] = per_channel.get(int(bot.get("channel") or 1), 0) + 1
    bot_channels = sorted(per_channel.items()) if len(per_channel) > 1 else []
    updater = update_status()
    updater["protected"] = current_settings.get("auth_enabled") == "1" and bool(session.get("seban_admin"))
    return render_template("manage.html", rates=read_rates(), ai_weights=read_ai_weights(), ai_weight_keys=AI_WEIGHT_KEYS, restart=restart_progress(), settings=current_settings, map_counts=map_counts, bot_count=len(bots), bot_channels=bot_channels, map_respawn_options=MAP_RESPAWN_OPTIONS, map_stone_respawn_ids=MAP_STONE_RESPAWN_IDS, map_respawn_status=read_map_regen_status(), server_settings=server_settings_status(), updater=updater, playerbots_release=playerbots_release_status(), update_csrf=update_csrf_token(), bot_count_wanted=read_bot_count() if CUSTOM_PATCHES_ENABLED else 0, spawn_plan=read_spawn_plan(), student_chest_disabled=read_student_chest_disabled() if CUSTOM_PATCHES_ENABLED else False, custom_patches_enabled=CUSTOM_PATCHES_ENABLED, include_real_players=include_real_players_in_rankings(), announce_plus9=read_announce_plus9_refines())


@app.post("/manage/update")
@login_required
def manage_update():
    current = settings()
    if current.get("auth_enabled") != "1" or not session.get("seban_admin"):
        flash("Aktualizacje z panelu wymagają włączonej ochrony hasłem.", "error")
        return redirect(url_for("manage"))
    supplied = request.form.get("update_csrf", "")
    expected = session.get("seban_update_csrf", "")
    if not expected or not hmac.compare_digest(supplied, expected):
        abort(403)
    try:
        queue_tieru_update(current.get("update_seban_panel") == "1")
    except (OSError, RuntimeError) as exc:
        flash(str(exc), "error")
    else:
        panel_note = " Razem z Playerbots zostanie zaktualizowany Seban Panel." if current.get("update_seban_panel") == "1" else " Seban Panel pozostanie w obecnej wersji."
        flash("Pobrano zlecenie aktualizacji. Serwer zostanie przebudowany przez odizolowany updater; postęp jest widoczny poniżej." + panel_note)
    return redirect(url_for("manage"))


@app.post("/manage/settings")
@login_required
def manage_settings():
    values, error = validate_display_settings(request.form)
    if error:
        flash(error, "error")
        return redirect(url_for("manage"))
    current = settings()
    enable_auth = request.form.get("auth_enabled") == "1"
    password = request.form.get("panel_password", "")
    if enable_auth:
        if password and len(password) < 8:
            flash("Nowe hasło musi mieć co najmniej 8 znaków.", "error")
            return redirect(url_for("manage"))
        password_hash = generate_password_hash(password) if password else current.get("auth_password_hash", "")
        if not password_hash:
            flash("Aby włączyć ochronę, ustaw hasło panelu.", "error")
            return redirect(url_for("manage"))
    else:
        password_hash = ""
        session.clear()
    values.update({"auth_enabled": "1" if enable_auth else "0", "auth_password_hash": password_hash, "setup_complete": "1"})
    write_settings(values)
    flash("Ustawienia panelu zapisane.")
    return redirect(url_for("manage"))


@app.post("/manage/overrides")
@login_required
def manage_overrides():
    values = {key: "1" if request.form.get(key) == "1" else "0" for key in ("allow_student_chest", "allow_moonlight_chest", "keep_demo_characters", "update_seban_panel")}
    write_settings(values)
    flash("Override'y zapisane. Zostaną zastosowane przy następnej aktualizacji Playerbots.")
    return redirect(url_for("manage"))


@app.post("/manage/restart-config")
@login_required
def manage_restart_config():
    action = request.form.get("submit_action", "apply")
    values, changes = {}, {}
    bot_count = None
    try:
        if action not in ("apply", "restart"):
            raise ValueError("Nieprawidłowa akcja.")
        if action == "apply":
            values = {name: int(request.form.get(name, "")) for name in RATE_NAMES}
            if any(not 1 <= value <= 10000 for value in values.values()):
                raise ValueError("Mnożniki muszą mieścić się w zakresie 1–10 000%.")
            # Older browser tabs opened before this field existed do not send
            # it -- leave the game side's current target alone rather than
            # snapping it to some default.
            if "playerbot_count" in request.form:
                bot_count = int(request.form["playerbot_count"])
                if not 1 <= bot_count <= 2500:
                    raise ValueError("Liczba botów musi mieścić się w zakresie 1–2500.")
            for index, name in MAP_RESPAWN_OPTIONS:
                for prefix in ("", "stone_"):
                    if prefix and index not in MAP_STONE_RESPAWN_IDS:
                        continue
                    key = f"{prefix}{index}"
                    # Older browser tabs do not contain the Metin fields.
                    if f"map_{key}" not in request.form:
                        continue
                    raw = request.form[f"map_{key}"].strip()
                    if not raw:
                        changes[key] = "reset"
                    else:
                        seconds = int(raw)
                        if not 1 <= seconds <= 3600:
                            raise ValueError(f"{name}: respawn musi mieścić się w zakresie 1–3600 sekund.")
                        changes[key] = seconds
        queue_server_settings(action, values, changes)
        if action == "apply" and bot_count is not None and CUSTOM_PATCHES_ENABLED:
            queue_botcount_change(bot_count)
    except ValueError as exc:
        flash(str(exc) if "invalid literal" not in str(exc) else "Wpisz całkowite wartości liczbowe.", "error")
    except RuntimeError as exc:
        flash(str(exc), "error")
    except FileExistsError:
        flash("Poprzednie zlecenie nadal trwa. Poczekaj na zakończenie restartu.", "error")
    except OSError:
        flash("Nie udało się zapisać zlecenia do kolejki gry.", "error")
    else:
        flash("Zestaw zapisany do kolejki: jeden restart zastosuje raty i respawn." if action == "apply"
              else "Zlecono restart bez zapisywania zmian w formularzu.")
    return redirect(url_for("manage"))


@app.post("/manage/spawn-plan")
@login_required
def manage_spawn_plan():
    # The request file this writes is read by Seban's own updater watcher,
    # which this image does not ship - the same gate as the bot count.
    if not CUSTOM_PATCHES_ENABLED:
        flash("Plan wejścia botów wymaga skryptów gry z integracji Sebana, których ten serwer nie ma; ustaw go w launcherze (LICZBA BOTÓW) albo w .env.", "error")
        return redirect(url_for("manage"))
    try:
        window = int(request.form.get("spawn_window_minutes", ""))
        late_joiners = int(request.form.get("late_joiners", ""))
        late_hours = int(request.form.get("late_join_hours", ""))
        if not 1 <= window <= 180:
            raise ValueError("Okno wejścia musi mieścić się w zakresie 1–180 minut.")
        if not 0 <= late_joiners <= 2500:
            raise ValueError("Liczba dodatkowych botów musi mieścić się w zakresie 0–2500.")
        if not 1 <= late_hours <= 168:
            raise ValueError("Okres późnego wejścia musi mieścić się w zakresie 1–168 godzin.")
        queue_spawn_plan(window, late_joiners, late_hours)
    except (TypeError, ValueError, OSError) as exc:
        flash(str(exc) or "Nie udało się zapisać planu wejścia.", "error")
    else:
        flash("Plan wejścia zapisany. Kontener gry zostanie odtworzony z nowymi ustawieniami.")
    return redirect(url_for("manage"))


@app.post("/manage/student-chest")
@login_required
def manage_student_chest():
    if not CUSTOM_PATCHES_ENABLED:
        flash("Przełącznik skrzyni startowej wymaga skryptów gry z integracji Sebana, których ten serwer nie ma.", "error")
        return redirect(url_for("manage"))
    disabled = "1" in request.form.getlist("disable_student_chest")
    write_student_chest_disabled(disabled)
    if disabled:
        flash("Skrzynia startowa jest teraz wyłączona dla nowych postaci graczy, każdej klasy — działa od razu, bez restartu.")
    else:
        flash("Skrzynia startowa jest teraz włączona dla nowych postaci graczy, każdej klasy — działa od razu, bez restartu.")
    return redirect(url_for("manage"))


@app.post("/manage/ranking-scope")
@login_required
def manage_ranking_scope():
    enabled = "1" in request.form.getlist("include_real_players")
    write_include_real_players_in_rankings(enabled)
    if enabled:
        flash("Rankingi i karuzela na dashboardzie liczą teraz każdą postać, nie tylko boty — działa od razu.")
    else:
        flash("Rankingi liczą teraz znowu wyłącznie boty.")
    return redirect(url_for("manage"))


@app.post("/manage/plus9-announce")
@login_required
def manage_plus9_announce():
    enabled = "1" in request.form.getlist("announce_plus9_refines")
    write_announce_plus9_refines(enabled)
    if enabled:
        flash("Ulepszenia graczy na +9 będą teraz ogłaszane na złoto na całym świecie — kolektor sprawdza co kilka sekund/minut, nie natychmiast.")
    else:
        flash("Ogłoszenia +9 wyłączone.")
    return redirect(url_for("manage"))


@app.post("/manage/restart-clear-stale")
@login_required
def manage_restart_clear_stale():
    support = server_settings_status()
    if not support["can_clear"]:
        flash("Nie można usunąć zlecenia: helper może jeszcze je przetwarzać albo zlecenie nie jest wystarczająco stare.", "error")
        return redirect(url_for("manage"))
    try:
        (RATES_SPOOL / "server-settings.request").unlink()
        (RATES_SPOOL / "server-settings.status").write_text(
            "state=failed\npercent=0\nmessage=Usunięto zaległe zlecenie bez aktywnego helpera gry.\ntime=%s\n" % int(time.time()), encoding="utf-8")
    except OSError:
        flash("Nie udało się usunąć zaległego zlecenia z kolejki.", "error")
    else:
        flash("Usunięto zaległe zlecenie. Zainstaluj integrację gry, zanim zlecisz kolejną zmianę.")
    return redirect(url_for("manage"))


@app.post("/manage/map-respawns")
@login_required
def manage_map_respawns():
    known_maps = {str(index) for index, _name in MAP_RESPAWN_OPTIONS}
    map_index = request.form.get("map_index", "")
    action = request.form.get("action", "")
    if map_index not in known_maps or action not in ("set", "reset"):
        flash("Nieprawidłowa mapa lub akcja.", "error")
        return redirect(url_for("manage"))
    seconds = None
    if action == "set":
        try:
            seconds = int(request.form.get("seconds", ""))
        except ValueError:
            seconds = 0
        if not 1 <= seconds <= 3600:
            flash("Czas respawnu musi mieścić się w zakresie 1–3600 sekund.", "error")
            return redirect(url_for("manage"))
    try:
        queue_map_regen_change(map_index, action, seconds)
    except OSError:
        flash("Nie udało się zapisać zmiany mapy do kolejki gry.", "error")
        return redirect(url_for("manage"))
    flash("Zmiana respawnu została zlecona. Gra zastosuje ją i uruchomi rdzenie ponownie.")
    return redirect(url_for("manage"))


@app.post("/manage/behavior")
@login_required
def manage_behavior():
    # Keep live-only switches even if this request came from an older browser
    # tab that does not render them yet.
    values = read_ai_weights()
    for key, _, _ in AI_WEIGHT_KEYS:
        try:
            value = int(request.form.get(key, AI_WEIGHT_NEUTRAL))
        except (TypeError, ValueError):
            value = AI_WEIGHT_NEUTRAL
        values[key] = max(AI_WEIGHT_MIN, min(AI_WEIGHT_MAX, value))
    values["CHAT"] = 1 if "1" in request.form.getlist("CHAT") else 0
    # Preserve the existing switch for a form opened before this field existed.
    values["BOOKS"] = 1 if "BOOKS" not in request.form else (1 if "1" in request.form.getlist("BOOKS") else 0)
    for key, default in (("LIFE", 0), ("WARS", 1), ("ISHOP", 1)):
        values[key] = default if key not in request.form else (1 if "1" in request.form.getlist(key) else 0)
    try:
        values["SCRAP"] = max(0, min(100, int(request.form.get("SCRAP", values.get("SCRAP", 0)))))
    except (TypeError, ValueError):
        values["SCRAP"] = 0
    try:
        values["REST"] = max(0, min(100, int(request.form.get("REST", values.get("REST", 100)))))
    except (TypeError, ValueError):
        values["REST"] = 100
    for key in ("CHEST", "CHEST_STONE"):
        if key not in request.form:
            continue
        try:
            values[key] = max(0, min(1000, int(request.form[key])))
        except (TypeError, ValueError):
            # A malformed chest control must not turn an existing server value
            # into a guessed default.
            continue
    try:
        write_ai_weights(values)
    except OSError:
        flash("Nie udało się zapisać wag Playerbots.", "error")
    else:
        flash("Zachowanie botów zapisane — nowy plan działania wejdzie w życie do 5 sekund, bez restartu.")
    return redirect(url_for("manage"))


@app.post("/manage/restart")
@login_required
def manage_restart():
    if request.form.get("confirmation", "").strip().upper() != "RESTART":
        flash("Aby potwierdzić restart, wpisz RESTART.", "error")
        return redirect(url_for("manage"))
    queue_rate_restart(read_rates())
    flash("Restart został zlecony. Pasek postępu pokaże kolejne etapy.")
    return redirect(url_for("manage"))


@app.route("/api/manage-status")
@login_required
def api_manage_status():
    status = read_rate_status()
    updater = update_status()
    updater["protected"] = settings().get("auth_enabled") == "1" and bool(session.get("seban_admin"))
    return {"ok": True, "restart": restart_progress(), "server_settings": server_settings_status(), "updater": updater, "rates": read_rates(), "bots": len(live_bots()), "maps": live_map_counts()}


@app.route("/api/heat-events")
@login_required
def api_heat_events():
    event_type = request.args.get("type", "deaths").strip().lower()
    event_types = {"deaths": "DEAD_BY_NPC", "metins": "STONE_KILL", "bosses": "BOSS_KILL"}
    how = event_types.get(event_type)
    if not how:
        abort(400)
    raw = rows("""SELECT l.x,l.y,l.time,p.name FROM log.log l LEFT JOIN player.player p ON p.id=l.who
        WHERE l.type='CHARACTER' AND l.how=%s AND l.time >= NOW() - INTERVAL 24 HOUR ORDER BY l.time DESC LIMIT 4000""", (how,))
    events = []
    for event in raw:
        for index, bound in MAP_BOUNDS.items():
            if bound[0] <= event["x"] < bound[0] + bound[2] and bound[1] <= event["y"] < bound[1] + bound[3]:
                events.append({"map_index": index, "x": event["x"], "y": event["y"], "time": event["time"].isoformat(), "name": event.get("name")})
                break
    return {"ok": True, "type": event_type, "events": events, "bounds": MAP_BOUNDS}


from item_grants import install as install_item_grants
install_item_grants(app, db, login_required, game_text)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=7789)
