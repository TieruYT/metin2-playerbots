# -*- coding: utf-8 -*-
"""The client's root scripts this line changes, rendered from the stock ones.

Usage:  python clientrootify.py --root <directory the stock root pack was extracted to>

    python tools/eterpack.py --profile mt2009 extract <Klient>/pack/root <dir>

Writes into client-root/ (beside serverinfo.py, which is hand-written):

  * gamerules.py  - RULES_VERSION bumped, so a client that accepted the public
                    server's terms is shown ours once (client-locale-src/rules.pl.txt);
  * intrologin.py - the three buttons of the login window: the home page is
                    the project's GitHub, the Discord is ours, and the Facebook
                    button - there is no Facebook - opens the buycoffee page;
                    a channel past the first is listed only while it answers;
  * uiitemshop.py, itemshop_subscriptionwindow.py - "Doladuj SM!" and the
                    subscription button open the buycoffee page, not mt2009.pl;
  * uisystem.py   - the system menu's support button opens our Discord.
  * uitooltip.py  - the GM branch no longer kills every item tooltip, and the
                    speed potion's asks for no apply name this client lacks.
  * game.py       - the "PlayerBotStatus" server command, handed to
                    playerbot_status_tail.py (hand-written, beside serverinfo.py);
                    Auto Lowy: the "AutoHuntTarget" and "AutoHuntLoot" commands, the K key and the
                    hunt among the updateables (uiautohunt.py, hand-written);
                    the "InventoryArrangeResult" command (inventoryarrange.py);
                    the ` key picks up every drop in range (pickupnearby.py).
  * uiinventory.py - the auto-stack button is "Scal i uporzadkuj": one
                    /inventory_arrange to the server, which pours the stacks
                    and lays the four pages out (inventoryarrange.py,
                    hand-written; playerbot_arrange.cpp on the server). The
                    method the button used to call stays under another name
                    and is never called.
  * offlineshopmanage.py - a click on an empty slot of the shop's edit grid
                    removes nothing instead of raising KeyError.
  * offlineshopsearch.py - a click on an icon of the item search's grid picks
                    that one item and "Szukaj" finds only the shops holding it
                    (Tyrion; the server half is playerbotify.py's
                    apply_shop_search_picked_item).
  * uigameoption.py, uiscript/gameoptiondialog.py - the "Tytuly botow" row of
                    the game options: a bot's personality title or the classic
                    alignment title (playerbot_status_tail.py keeps the choice).
  * uiscript/inventorywindow.py - four page tabs instead of two; uiinventory.py
                    already makes one tab per page but the horse page, reading
                    player.INVENTORY_PAGE_COUNT from the exe.
  * utils.py      - the requirement counts (a horse-bag slot, a special
                    shop) read all four bag pages and the horse page.
  * constinfo.py  - GAME_VERSION 1.1.0: the version the client sends before
                    logging in, and the server's server_version refuses the
                    two-page client below it (m2-render-config).
  * uirefine.py   - a refine window that fails to open sends the cancel, so
                    the server's refine mode does not lock the bag, and its
                    two texts fall back to the plain numbers.
  * localeinfo.py - English over the loaded texts (english_gui.py), keeping
                    a typed line a function.

Exact-string edits on the stock CP1250/CRLF files, byte for byte otherwise.
Idempotent; re-run after a new client package.
"""
import argparse
import io
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, '..', 'client-root'))

# english_gui's texts over the loaded ones, keeping a typed line (SA, SNA...)
# a function: the scripts call those, and a plain string where they call one
# raises. SA when the English text takes an argument, SNA when it does not.
ENGLISH_OVERLAY = (
    b'if systemSetting.GetLanguage() == "en":\r\n'
    b'\timport english_gui\r\n'
    b'\tfor _key, _text in english_gui.GAME.items():\r\n'
    b'\t\tif callable(globals().get(_key)):\r\n'
    b"\t\t\t_text = ('%' in _text.replace('%%', '') and SA or SNA)(_text)\r\n"
    b'\t\tglobals()[_key] = _text\r\n')

EDITS = {
    # --- Osiem jezykow (Lostek) i angielski interfejs (Codex), 2.0.23 -------
    # Wybor jezyka przy logowaniu mial dwie pozycje; paczka locale Lostka ma
    # osiem katalogow (pl en de es it pt ro tr), wiec okno konfiguracji tez.
    'configmain.py': [
        (b'\t\tLANGUAGE_OPTIONS = [uiScriptLocale.LANGUAGE_OPTION_DEFAULT,\r\n'
         b'\t\t\t\t\t\t\tuiScriptLocale.LANGUAGE_OPTION_ENGLISH]\r\n',
         b'\t\t# Dostepne locale znajdujace sie w locale/locale/\r\n'
         b'\t\tLANGUAGE_OPTIONS = [\r\n'
         b'\t\t\t"Polski",\r\n'
         b'\t\t\t"English",\r\n'
         b'\t\t\t"Deutsch",\r\n'
         b'\t\t\t"Espanol",\r\n'
         b'\t\t\t"Italiano",\r\n'
         b'\t\t\t"Portugues",\r\n'
         b'\t\t\t"Romana",\r\n'
         b'\t\t\t"Turkce",\r\n'
         b'\t\t]\r\n'),
        (b'\t\t\tif index == 1:\r\n'
         b'\t\t\t\treturn "en"\r\n'
         b'\t\t\telse:\r\n'
         b'\t\t\t\treturn "pl"\r\n'
         b'\r\n'
         b'\t\tdef __LanguageCodeToIndex(self, lang):\r\n'
         b'\t\t\tif lang == "en":\r\n'
         b'\t\t\t\treturn 1\r\n'
         b'\t\t\telse:\r\n',
         b'\t\t\tlanguage_codes = ["pl", "en", "de", "es", "it", "pt", "ro", "tr"]\r\n'
         b'\t\t\tif index < 0 or index >= len(language_codes):\r\n'
         b'\t\t\t\treturn "pl"\r\n'
         b'\t\t\treturn language_codes[index]\r\n'
         b'\r\n'
         b'\t\tdef __LanguageCodeToIndex(self, lang):\r\n'
         b'\t\t\tlanguage_codes = ["pl", "en", "de", "es", "it", "pt", "ro", "tr"]\r\n'
         b'\t\t\ttry:\r\n'
         b'\t\t\t\treturn language_codes.index(lang)\r\n'
         b'\t\t\texcept ValueError:\r\n'),
    ],
    # Angielskie GUI: 281 napisow w english_gui.py (reczny plik obok
    # serverinfo.py) nalozonych na wczytane tablice tylko dla "en". Klucze,
    # ktorych locale_interface.txt EN nie ma, zostaja po polsku w kazdym innym
    # jezyku dzieki setdefault ponizej - wiec DE/ES/IT/PT/RO/TR nic nie traca.
    'localeinfo.py': [
        # Migration of the overlay's first form (client 2.0.9 to 2.0.25): it
        # put english_gui's plain texts over typed lines too, and a typed line
        # is a function the scripts call - WHISPER_ERROR[mode](name) - so an
        # English player whispering someone they had blocked got "'str' object
        # is not callable" instead of the message.
        (b'if systemSetting.GetLanguage() == "en":\r\n'
         b'\timport english_gui\r\n'
         b'\tglobals().update(english_gui.GAME)\r\n',
         ENGLISH_OVERLAY,
         True),
        (b'\r\n'
         b'\r\n'
         b'if app.ENABLE_CHEQUE_SYSTEM:\r\n'
         b'\tdef NumberToGold(n) :\r\n',
         b'\r\n'
         b'\r\n'
         + ENGLISH_OVERLAY +
         b'\r\n'
         b'if app.ENABLE_CHEQUE_SYSTEM:\r\n'
         b'\tdef NumberToGold(n) :\r\n'),
    ],
    'uiscriptlocale.py': [
        (b'\tTryLoadLocaleFile("%s/locale_interface_ex.txt" % app.GetLocalePath())\r\n',
         b'\tTryLoadLocaleFile("%s/locale_interface_ex.txt" % app.GetLocalePath())\r\n'
         b'\r\n'
         b'# Napisy, ktore do tej pory staly w skryptach na sztywno. Wartosc\r\n'
         b'# domyslna jest polska, wiec kazdy inny jezyk widzi to, co dotad.\r\n'
         b"globals().setdefault('BOT_TITLES_LABEL', 'Tytu\xb3y bot\xf3w')\r\n"
         b"globals().setdefault('BOT_TITLES_PERSONALITY', 'Osobowo\x9c\xe6')\r\n"
         b"globals().setdefault('BOT_TITLES_OFF', 'Wy\xb3\xb9czone')\r\n"
         b"globals().setdefault('SYSTEM_VERSION', 'Wersja: %d.%d.%d%s')\r\n"
         b"globals().setdefault('INVENTORY_SORT_STACK', 'Scal i uporz\xb9dkuj')\r\n"
         b"globals().setdefault('INVENTORY_PAGE_BUTTON_TOOLTIP_3', '3. Ekwipunek')\r\n"
         b"globals().setdefault('INVENTORY_PAGE_BUTTON_TOOLTIP_4', '4. Ekwipunek')\r\n"
         b"globals().setdefault('CHARACTER_STATUS_TITLE', 'Status postaci')\r\n"
         b"globals().setdefault('CHARACTER_ATTRIBUTES_TITLE', 'Atrybuty')\r\n"
         b"globals().setdefault('ITEMSHOP_CATEGORIES', 'Kategorie')\r\n"
         b"globals().setdefault('ITEMSHOP_ACCOUNT_STATE', 'Stan Konta')\r\n"
         b'\r\n'
         b'if systemSetting.GetLanguage() == "en":\r\n'
         b'\timport english_gui\r\n'
         b'\tglobals().update(english_gui.UI)\r\n'),
    ],
    'uiscript/systemdialog.py': [
        (b'\t\t\t\t\t"text": "Wersja: 1.0.0",\r\n',
         b'\t\t\t\t\t"text": uiScriptLocale.SYSTEM_VERSION % (1, 0, 0, ""),\r\n'),
    ],
    # Naglowki okna postaci i sklepu z przedmiotami: widac je na kazdym
    # zrzucie z angielskiego klienta, bo stoja w skrypcie, nie w locale.
    'uiscript/characterwindow.py': [
        (b'"text" : "Status postaci",',
         b'"text" : uiScriptLocale.CHARACTER_STATUS_TITLE,'),
        (b'"text" : "Atrybuty",',
         b'"text" : uiScriptLocale.CHARACTER_ATTRIBUTES_TITLE,'),
    ],
    'uiscript/itemshopwindow.py': [
        (b'\t\t\t\t\t\t\t\t\t"text" : "Kategorie",\r\n',
         b'\t\t\t\t\t\t\t\t\t"text" : uiScriptLocale.ITEMSHOP_CATEGORIES,\r\n'),
        (b'\t\t\t\t\t\t\t\t\t"text" : "Stan Konta",\r\n',
         b'\t\t\t\t\t\t\t\t\t"text" : uiScriptLocale.ITEMSHOP_ACCOUNT_STATE,\r\n'),
    ],
    'gamerules.py': [
        (b'RULES_VERSION = 3\r\n', b'RULES_VERSION = 4\r\n'),
    ],
    # The ItemShop's "Doladuj SM!" and the subscription window's button both
    # opened the public server's site, and the system menu's support button
    # its account page. Nothing on this server sells coins; the two shop
    # buttons open the buycoffee page (the players' own suggestion) and
    # support is the Discord.
    'uiitemshop.py': [
        (b'\t\t\t"type" : "open_url",\r\n\t\t\t"value" : "https://mt2009.pl/"\r\n',
         b'\t\t\t"type" : "open_url",\r\n\t\t\t"value" : "https://buycoffee.to/metin2-playerbots"\r\n'),
    ],
    'itemshop_subscriptionwindow.py': [
        (b'\t\tutils.open_url("https://mt2009.pl/")\r\n',
         b'\t\tutils.open_url("https://buycoffee.to/metin2-playerbots")\r\n'),
    ],
    # A game master saw no item tooltip at all: the GM branch of the item
    # tooltip iterates self.auxiliaryDict.items(), and auxiliaryDict is the
    # empty string (its assignment from player.GetAuxiliaryString is
    # commented out), so every tooltip died in AttributeError before
    # ShowToolTip ("Nie widac nazw itemow" - "tylko gdy jestes GM").
    'uitooltip.py': [
        (b'\t\t\tself.AppendTextLine("Auxs: ")\r\n'
         b'\t\t\tfor _, val in self.auxiliaryDict.items():\r\n'
         b'\t\t\t\tself.AppendTextLine("Key: [{}] Value: [{}]".format(_, val))\r\n',
         b'\t\t\tif isinstance(self.auxiliaryDict, dict) and self.auxiliaryDict:\r\n'
         b'\t\t\t\tself.AppendTextLine("Auxs: ")\r\n'
         b'\t\t\t\tfor _, val in self.auxiliaryDict.items():\r\n'
         b'\t\t\t\t\tself.AppendTextLine("Key: [{}] Value: [{}]".format(_, val))\r\n'),
        # The speed potion's tooltip asked item for APPLY_ATT_SPEED and
        # APPLY_MOV_SPEED, which this client's item module does not export
        # (AttributeError in l0st3k's syserr, 15 September), so hovering one
        # broke the tooltip. On this line an apply is its point: 17 and 19,
        # what Zielona and Fioletowa Mikstura carry in value0.
        (b'\t\tif abilityType == item.APPLY_ATT_SPEED:\r\n',
         b'\t\tif abilityType == getattr(item, "APPLY_ATT_SPEED", 17):\r\n'),
        (b'\t\telif abilityType == item.APPLY_MOV_SPEED:\r\n',
         b'\t\telif abilityType == getattr(item, "APPLY_MOV_SPEED", 19):\r\n'),
    ],
    # A bot's status arrives as the command "PlayerBotStatus <vid> <hex>"
    # (SendPlayerBotOverheadChat) and is drawn as a text tail only: as talking
    # it also went into the chat history, and a town of bots filled the window.
    # Both edits take in the line after the insertion, so a second run on our
    # own output finds neither anchor and changes nothing.
    'game.py': [
        (b'\t\t\t"PlayerbotOverhead"\t\t\t\t: self.__PlayerbotAdmin_Overhead,\r\n'
         b'\t\t\t"Top1Badge"\t\t\t\t\t\t\t: self.__OnTop1Badge,\r\n',
         b'\t\t\t"PlayerbotOverhead"\t\t\t\t: self.__PlayerbotAdmin_Overhead,\r\n'
         b'\t\t\t"PlayerBotStatus"\t\t\t\t: self.__PlayerBotStatus,\r\n'
         b'\t\t\t"Top1Badge"\t\t\t\t\t\t\t: self.__OnTop1Badge,\r\n'),
        (b'\t\t\t\tself.interface.wndPlayerbotAdmin.OnOverheadTail(vid, wire)\r\n'
         b'\r\n'
         b'\t# Same transport as PlayerbotOverhead above (SendPlayerBotOverheadTail),\r\n',
         b'\t\t\t\tself.interface.wndPlayerbotAdmin.OnOverheadTail(vid, wire)\r\n'
         b'\r\n'
         b'\tdef __PlayerBotStatus(self, vid="0", encodedText="", *rest):\r\n'
         b'\t\t# A bot\'s status as a text tail and nothing in the chat history\r\n'
         b'\t\t# (playerbot_status_tail.py; SendPlayerBotOverheadChat on the server).\r\n'
         b'\t\timport playerbot_status_tail\r\n'
         b'\t\tplayerbot_status_tail.show(vid, encodedText, *rest)\r\n'
         b'\r\n'
         b'\t# Same transport as PlayerbotOverhead above (SendPlayerBotOverheadTail),\r\n'),
        # Auto Lowy (uiautohunt.py, hand-written beside serverinfo.py): the
        # server names the target with "AutoHuntTarget <vid>", K opens the
        # window, and the hunt runs as one of the game's updateables. Every
        # insertion splits its own anchor, so a re-run on our output finds the
        # new text and not the old, and none of them touches the two above.
        (b'\t\t\t"Top1Badge"\t\t\t\t\t\t\t: self.__OnTop1Badge,\r\n'
         b'\r\n'
         b'\t\t\t# fishing\r\n',
         b'\t\t\t"Top1Badge"\t\t\t\t\t\t\t: self.__OnTop1Badge,\r\n'
         b'\t\t\t"AutoHuntTarget"\t\t\t\t: self.__AutoHuntTarget,\r\n'
         b'\t\t\t"AutoHuntLoot"\t\t\t\t\t: self.__AutoHuntLoot,\r\n'
         b'\r\n'
         b'\t\t\t# fishing\r\n'),
        (b'\t\t#onPressKeyDict[app.DIK_K]\t\t\t= lambda : self.interface.OpenCubeWindow()\r\n'
         b'\t\t# CUBE_TEST_END\r\n',
         b'\t\t#onPressKeyDict[app.DIK_K]\t\t\t= lambda : self.interface.OpenCubeWindow()\r\n'
         b'\t\tonPressKeyDict[app.DIK_K]\t\t\t= lambda : self.__ToggleAutoHunt()\r\n'
         b'\t\t# CUBE_TEST_END\r\n'),
        (b'\t\tself.RegisterUpdatable(updateable.PickUpOnDownKey())\r\n'
         b'\r\n',
         b'\t\tself.RegisterUpdatable(updateable.PickUpOnDownKey())\r\n'
         b'\t\timport uiautohunt\r\n'
         b'\t\tself.RegisterUpdatable(uiautohunt.GetHunter())\r\n'
         b'\r\n'),
        (b'\t\tself.__PressQuickSlot(5)\r\n'
         b'\t\treturn\r\n'
         b'\r\n'
         b'\tdef __ToggleSprint(self):\r\n',
         b'\t\tself.__PressQuickSlot(5)\r\n'
         b'\t\treturn\r\n'
         b'\r\n'
         b'\tdef __ToggleAutoHunt(self):\r\n'
         b'\t\timport uiautohunt\r\n'
         b'\t\tuiautohunt.ToggleWindow()\r\n'
         b'\r\n'
         b'\tdef __AutoHuntTarget(self, vid="0", *rest):\r\n'
         b'\t\timport uiautohunt\r\n'
         b'\t\tuiautohunt.OnServerTarget(vid)\r\n'
         b'\r\n'
         b'\tdef __AutoHuntLoot(self, vid="0", x="0", y="0", *rest):\r\n'
         b'\t\timport uiautohunt\r\n'
         b'\t\tuiautohunt.OnServerLoot(vid, x, y)\r\n'
         b'\r\n'
         b'\tdef __ToggleSprint(self):\r\n'),
        # A bot's personality in the place of its alignment title: the server's
        # "PlayerBotTitle <vid> <personality>" (ManagePlayerBotPersonalityTitle)
        # goes to playerbot_status_tail.py, and the keeper that puts the title
        # back after an alignment refresh joins the updateables with the first
        # one. The entry sits before the admin window's block and the handler
        # before the achievements' end - places no other edit here touches, and
        # each insertion splits its own anchor.
        (b'\t\t\t"GMPanelSetAIWeightResult"\t: self.__GMPanelSetAIWeightResult,\r\n'
         b'\r\n'
         b'\t\t\t"OpenPlayerbotAdminWindow"\t\t\t: self.__PlayerbotAdmin_Open,\r\n',
         b'\t\t\t"GMPanelSetAIWeightResult"\t: self.__GMPanelSetAIWeightResult,\r\n'
         b'\r\n'
         b'\t\t\t"PlayerBotTitle"\t\t\t\t: self.__PlayerBotTitle,\r\n'
         b'\t\t\t"OpenPlayerbotAdminWindow"\t\t\t: self.__PlayerbotAdmin_Open,\r\n'),
        (b'\t\t\tself.interface.wndPlayerbotAdmin.OnAchievementRow(id, pid, name)\r\n'
         b'\r\n'
         b'\tdef __PlayerbotAdmin_AchievementsEnd(self):\r\n',
         b'\t\t\tself.interface.wndPlayerbotAdmin.OnAchievementRow(id, pid, name)\r\n'
         b'\r\n'
         b'\tdef __PlayerBotTitle(self, vid="0", personality="-1", *rest):\r\n'
         b'\t\t# A bot\'s personality where a player\'s alignment title stands\r\n'
         b'\t\t# (playerbot_status_tail.py; ManagePlayerBotPersonalityTitle on the server).\r\n'
         b'\t\timport playerbot_status_tail\r\n'
         b'\t\tif playerbot_status_tail.show_title(vid, personality) and not getattr(self, "playerbotTitleKeeper", None):\r\n'
         b'\t\t\tself.playerbotTitleKeeper = playerbot_status_tail.GetTitleKeeper()\r\n'
         b'\t\t\tself.RegisterUpdatable(self.playerbotTitleKeeper)\r\n'
         b'\r\n'
         b'\tdef __PlayerbotAdmin_AchievementsEnd(self):\r\n'),
        # "Scal i uporzadkuj" answers "InventoryArrangeResult <code> <moved>
        # <merged> <units>" (playerbot_arrange.cpp) and inventoryarrange.py says
        # what happened. The entry follows the fishing block and the handler
        # precedes the ItemShop's: text no other edit here reads or writes.
        (b'\t\t\t"FishingGameEvent": self.FishingGameEvent,\r\n',
         b'\t\t\t"FishingGameEvent": self.FishingGameEvent,\r\n'
         b'\r\n'
         b'\t\t\t# "Scal i uporzadkuj" (inventoryarrange.py)\r\n'
         b'\t\t\t"InventoryArrangeResult"\t: self.__InventoryArrangeResult,\r\n'),
        (b'\tdef __InGameShop_Show(self, url):\r\n',
         b'\tdef __InventoryArrangeResult(self, code="0", moved="0", merged="0", units="0", *rest):\r\n'
         b'\t\timport inventoryarrange\r\n'
         b'\t\tinventoryarrange.OnResult(code, moved, merged, units)\r\n'
         b'\r\n'
         b'\tdef __InGameShop_Show(self, url):\r\n'),
        # "Podnies caly drop" (vanderro, 18 September; Tieru: "jedno Z niech
        # bedzie klasycznie, a ` najwyzej jako caly drop"): ` asks the server
        # for every drop in range (pickupnearby.py, hand-written;
        # CHARACTER::PickupNearbyItems), Z keeps the single pickup. The key's
        # line is replaced whole, and the method goes after PickUpItem, whose
        # own lines are the anchor and stay as they are.
        (b'\t\tonPressKeyDict[app.DIK_GRAVE]\t\t= lambda : self.PickUpItem()\r\n',
         b'\t\tonPressKeyDict[app.DIK_GRAVE]\t\t= lambda : self.PickUpNearbyItems()\r\n'),
        (b'\tdef PickUpItem(self):\r\n'
         b'\t\tplayer.PickCloseItem()\r\n'
         b'\r\n',
         b'\tdef PickUpItem(self):\r\n'
         b'\t\tplayer.PickCloseItem()\r\n'
         b'\r\n'
         b'\tdef PickUpNearbyItems(self):\r\n'
         b'\t\timport pickupnearby\r\n'
         b'\t\tpickupnearby.Request()\r\n'
         b'\r\n'),
        # The safebox's answers (safeboxtransfer.py): its "Scal i uporzadkuj",
        # and a stack moved by count. The entry follows the bag's own, which
        # ends its line; the handlers go after the Top1 badge's, whose end no
        # other edit reads - placed after the bag's handler they would have
        # split the text by which the edit above knows it has been applied, and
        # a second run would have added that handler again.
        (b'\t\t\t"InventoryArrangeResult"\t: self.__InventoryArrangeResult,\r\n',
         b'\t\t\t"InventoryArrangeResult"\t: self.__InventoryArrangeResult,\r\n'
         b'\t\t\t"SafeboxArrangeResult"\t: self.__SafeboxArrangeResult,\r\n'
         b'\t\t\t"SafeboxTransferResult"\t: self.__SafeboxTransferResult,\r\n'),
        (b'\t\t\tself.interface.wndTop1Badge.Refresh(vid)\r\n'
         b'\r\n',
         b'\t\t\tself.interface.wndTop1Badge.Refresh(vid)\r\n'
         b'\r\n'
         b'\tdef __SafeboxArrangeResult(self, code="0", moved="0", merged="0", units="0", *rest):\r\n'
         b'\t\timport safeboxtransfer\r\n'
         b'\t\tsafeboxtransfer.OnArrangeResult(code, moved, merged, units)\r\n'
         b'\r\n'
         b'\tdef __SafeboxTransferResult(self, op="0", code="0", units="0", *rest):\r\n'
         b'\t\timport safeboxtransfer\r\n'
         b'\t\tsafeboxtransfer.OnTransferResult(op, code, units)\r\n'
         b'\r\n'),
    ],
    # "Scal i uporzadkuj" (Tieru, 18 September; Codex's audit the same day):
    # the inventory's auto-stack button asks the server once
    # (inventoryarrange.py) instead of sending a move for every pair of stacks.
    # Those moves were three hundred in a frame - the flood limit closed the
    # connection on them ("loga postac do ekranu logowania", l0st3k, 15
    # September) - and spread out a few at a time (autostackpump.py) they
    # could still only pour stacks, never lay a page out. Only the method's
    # first line is the anchor, and the old body stays under another name,
    # never called: the same edit applies to the stock root, to a published
    # root that carries the pump, and to one that carries this.
    'uiinventory.py': [
        (b'\tdef __OnAutoStackButton(self):\r\n',
         b'\tdef __OnAutoStackButton(self):\r\n'
         b'\t\timport inventoryarrange\r\n'
         b'\t\tinventoryarrange.Request()\r\n'
         b'\r\n'
         b'\tdef __OnAutoStackButtonByMoves(self):\r\n'),
        # A stack from the safebox dropped on the bag (blasty, 19 September;
        # safeboxtransfer.py): a part of it, or onto the same item, goes to the
        # server as a command with the count; a whole stack onto a free cell
        # keeps the packet. Dropped on a stack in the bag it used to do nothing.
        (b'\t\t\t\telse:\r\n'
         b'\t\t\t\t\tnet.SendSafeboxCheckoutPacket(attachedSlotPos, selectedSlotPos)\r\n',
         b'\t\t\t\telse:\r\n'
         b'\t\t\t\t\timport safeboxtransfer\r\n'
         b'\t\t\t\t\tsafeboxtransfer.DropIntoBag(attachedSlotPos, selectedSlotPos, attachedItemCount, False)\r\n'),
        (b'\t\t\t\tself.__DropSrcItemToDestItemInInventory(attachedItemVID, attachedSlotPos, itemSlotIndex)\r\n'
         b'\r\n'
         b'\t\t\tmouseModule.mouseController.DeattachObject()\r\n',
         b'\t\t\t\tself.__DropSrcItemToDestItemInInventory(attachedItemVID, attachedSlotPos, itemSlotIndex)\r\n'
         b'\r\n'
         b'\t\t\telif player.SLOT_TYPE_SAFEBOX == attachedSlotType and player.ITEM_MONEY != attachedItemVID:\r\n'
         b'\t\t\t\timport safeboxtransfer\r\n'
         b'\t\t\t\tsafeboxtransfer.DropIntoBag(attachedSlotPos, itemSlotIndex, mouseModule.mouseController.GetAttachedItemCount(), True)\r\n'
         b'\r\n'
         b'\t\t\tmouseModule.mouseController.DeattachObject()\r\n'),
    ],
    # The safebox's side (blasty's proposal, 19 September; Tieru: "Jasne"):
    # "Scal i uporzadkuj" in the title bar, Shift and a click to split one of its
    # stacks, and a stack dropped on the same item poured into it - bag into
    # safebox, safebox into bag and inside the safebox (safeboxtransfer.py,
    # hand-written; playerbot_arrange.cpp on the server). The drop onto a taken
    # place from the bag was a commented-out packet; it is a command with the
    # count now, and the packets stay for whole stacks onto free places.
    'uisafebox.py': [
        (b'import item\r\n'
         b'\r\n'
         b'EVENT_QUICK_REMOVE_SAFEBOX_ITEM',
         b'import item\r\n'
         b'import safeboxtransfer\r\n'
         b'\r\n'
         b'EVENT_QUICK_REMOVE_SAFEBOX_ITEM'),
        (b'\t\tif self.dlgPickMoney:\r\n'
         b'\t\t\tself.dlgPickMoney.Destroy()\r\n'
         b'\t\t\tself.dlgPickMoney = None\r\n',
         b'\t\tif self.dlgPickMoney:\r\n'
         b'\t\t\tself.dlgPickMoney.Destroy()\r\n'
         b'\t\t\tself.dlgPickMoney = None\r\n'
         b'\t\tif getattr(self, "dlgPickItem", None):\r\n'
         b'\t\t\tself.dlgPickItem.Destroy()\r\n'
         b'\t\t\tself.dlgPickItem = None\r\n'),
        (b'\t\tself.GetChild("ChangePasswordButton").SetEvent(ui.__mem_func__(self.OnChangePassword))\r\n'
         b'\t\tself.GetChild("ExitButton").SetEvent(ui.__mem_func__(self.Close))\r\n',
         b'\t\tself.GetChild("ChangePasswordButton").SetEvent(ui.__mem_func__(self.OnChangePassword))\r\n'
         b'\t\tself.GetChild("ExitButton").SetEvent(ui.__mem_func__(self.Close))\r\n'
         b'\t\t# "Scal i uporzadkuj" (uiscript/safeboxwindow.py) and the count a\r\n'
         b'\t\t# stack is split with (safeboxtransfer.py).\r\n'
         b'\t\ttry:\r\n'
         b'\t\t\tself.GetChild("ArrangeButton").SetEvent(ui.__mem_func__(self.__OnArrangeButton))\r\n'
         b'\t\texcept KeyError:\r\n'
         b'\t\t\tpass\r\n'
         b'\t\tself.dlgPickItem = safeboxtransfer.MakePickDialog(ui.__mem_func__(self.__OnPickItem))\r\n'),
        (b'\t\tself.dlgPickMoney.Close()\r\n'
         b'\t\tself.dlgChangePassword.Close()\r\n'
         b'\t\tself.Hide()\r\n',
         b'\t\tself.dlgPickMoney.Close()\r\n'
         b'\t\tself.dlgChangePassword.Close()\r\n'
         b'\t\tif getattr(self, "dlgPickItem", None):\r\n'
         b'\t\t\tself.dlgPickItem.Close()\r\n'
         b'\t\tself.Hide()\r\n'),
        (b'\t\t\tif player.SLOT_TYPE_SAFEBOX == attachedSlotType:\r\n'
         b'\r\n'
         b'\t\t\t\tnet.SendSafeboxItemMovePacket(attachedSlotPos, selectedSlotPos)\r\n'
         b'\t\t\t\t#snd.PlaySound("sound/ui/drop.wav")\r\n',
         b'\t\t\tif player.SLOT_TYPE_SAFEBOX == attachedSlotType:\r\n'
         b'\r\n'
         b'\t\t\t\tsafeboxtransfer.DropInSafebox(attachedSlotPos, selectedSlotPos, mouseModule.mouseController.GetAttachedItemCount(), False)\r\n'
         b'\t\t\t\t#snd.PlaySound("sound/ui/drop.wav")\r\n'),
        (b'\t\t\t\t\tself.AddItemToSafebox(attachedInvenType, attachedSlotPos, selectedSlotPos)\r\n',
         b'\t\t\t\t\tsafeboxtransfer.DropIntoSafebox(attachedInvenType, attachedSlotPos, selectedSlotPos, mouseModule.mouseController.GetAttachedItemCount(), False)\r\n'),
        (b'\t\t\t\t\tattachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()\r\n'
         b'\t\t\t\t\t#net.SendSafeboxCheckinPacket(attachedSlotPos, selectedSlotPos)\r\n',
         b'\t\t\t\t\tattachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()\r\n'
         b'\t\t\t\t\tsafeboxtransfer.DropIntoSafebox(player.INVENTORY, attachedSlotPos, selectedSlotPos, mouseModule.mouseController.GetAttachedItemCount(), True)\r\n'),
        (b'\t\t\telif player.SLOT_TYPE_SAFEBOX == attachedSlotType:\r\n'
         b'\t\t\t\tattachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()\r\n'
         b'\t\t\t\tnet.SendSafeboxItemMovePacket(attachedSlotPos, selectedSlotPos)\r\n',
         b'\t\t\telif player.SLOT_TYPE_SAFEBOX == attachedSlotType:\r\n'
         b'\t\t\t\tattachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()\r\n'
         b'\t\t\t\tsafeboxtransfer.DropInSafebox(attachedSlotPos, selectedSlotPos, mouseModule.mouseController.GetAttachedItemCount(), True)\r\n'),
        (b'\t\t\t\tchat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_BUY_INFO)\r\n'
         b'\r\n'
         b'\t\t\telse:\r\n'
         b'\t\t\t\tselectedItemID = safebox.GetItemID(selectedSlotPos)\r\n',
         b'\t\t\t\tchat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_BUY_INFO)\r\n'
         b'\r\n'
         b'\t\t\telif app.IsPressed(app.DIK_LSHIFT) and safebox.GetItemCount(selectedSlotPos) > 1:\r\n'
         b'\t\t\t\tself.__OpenPickItem(selectedSlotPos)\r\n'
         b'\r\n'
         b'\t\t\telse:\r\n'
         b'\t\t\t\tselectedItemID = safebox.GetItemID(selectedSlotPos)\r\n'),
        (b'\tdef RemoveItemFromSafebox(self, slotPos):\r\n'
         b'\t\tpass\r\n',
         b'\tdef RemoveItemFromSafebox(self, slotPos):\r\n'
         b'\t\tpass\r\n'
         b'\r\n'
         b'\tdef __OnArrangeButton(self):\r\n'
         b'\t\tsafeboxtransfer.RequestArrange()\r\n'
         b'\r\n'
         b'\tdef __OpenPickItem(self, slotPos):\r\n'
         b'\t\tself.dlgPickItem.SetTitleName(localeInfo.PICK_ITEM_TITLE)\r\n'
         b'\t\tself.dlgPickItem.Open(safebox.GetItemCount(slotPos))\r\n'
         b'\t\tself.dlgPickItem.itemGlobalSlotIndex = slotPos\r\n'
         b'\r\n'
         b'\tdef __OnPickItem(self, count, *rest):\r\n'
         b'\t\tslotPos = self.dlgPickItem.itemGlobalSlotIndex\r\n'
         b'\t\tmouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_SAFEBOX, slotPos, safebox.GetItemID(slotPos), count)\r\n'
         b'\t\tsnd.PlaySound("sound/ui/pick.wav")\r\n'),
    ],
    # The safebox's "Scal i uporzadkuj": the bag's button and images, in the
    # title bar where the bag has its own.
    'uiscript/safeboxwindow.py': [
        (b'import uiScriptLocale\r\n'
         b'\r\n'
         b'window = {\r\n',
         b'import uiScriptLocale\r\n'
         b'import flamewindPath\r\n'
         b'\r\n'
         b'window = {\r\n'),
        (b'\t\t\t\t\t\t{ "name":"TitleName", "type":"text", "x":77, "y":3, "text":uiScriptLocale.SAFE_TITLE, "text_horizontal_align":"center" },\r\n'
         b'\t\t\t\t\t),\r\n',
         b'\t\t\t\t\t\t{ "name":"TitleName", "type":"text", "x":77, "y":3, "text":uiScriptLocale.SAFE_TITLE, "text_horizontal_align":"center" },\r\n'
         b'\r\n'
         b'\t\t\t\t\t\t{\r\n'
         b'\t\t\t\t\t\t\t"name" : "ArrangeButton",\r\n'
         b'\t\t\t\t\t\t\t"type" : "button",\r\n'
         b'\t\t\t\t\t\t\t"x" : 42,\r\n'
         b'\t\t\t\t\t\t\t"y" : -1,\r\n'
         b'\t\t\t\t\t\t\t"horizontal_align": "right",\r\n'
         b'\t\t\t\t\t\t\t"vertical_align": "center",\r\n'
         b'\t\t\t\t\t\t\t"default_image" : flamewindPath.GetInventory("autostack_01"),\r\n'
         b'\t\t\t\t\t\t\t"over_image" : flamewindPath.GetInventory("autostack_02"),\r\n'
         b'\t\t\t\t\t\t\t"down_image" : flamewindPath.GetInventory("autostack_03"),\r\n'
         b'\t\t\t\t\t\t\t"tooltip_text" : "Scal i uporz\\xb9dkuj",\r\n'
         b'\t\t\t\t\t\t\t"tooltip_y": -19,\r\n'
         b'\t\t\t\t\t\t\t"tooltip_x": -30,\r\n'
         b'\t\t\t\t\t\t},\r\n'
         b'\t\t\t\t\t),\r\n'),
    ],
    # Tyrion's item search (22 September, tested on 2.0.94): a click on an
    # icon of the search window's grid picks that one item - a second click
    # drops it - and "Szukaj" then finds only the shops holding it. The
    # item goes to the server in SendSearchItem's second field as
    # vnum * 1000 + its socket0, 0 being the whole category as before, so the
    # packet is unchanged (playerbotify.py, apply_shop_search_picked_item).
    'offlineshopsearch.py': [
        (b'def is_wearable_search(category):\r\n'
         b'\treturn category in (ikashop.SHOP_SEARCH_CATEGORY_ARMOR, ikashop.SHOP_SEARCH_CATEGORY_WEAPON, ikashop.SHOP_SEARCH_CATEGORY_JEWELRY)\r\n'
         b'\r\n'
         b'def search_category(category, sub_category=-1, isSearchAttr=False):\r\n'
         b'\tprint category, sub_category\r\n'
         b'\tconstInfo.OFFLINESHOP_LAST_SEARCHED_CATEGORY = category\r\n'
         b'\tconstInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY = sub_category\r\n',
         b'def is_wearable_search(category):\r\n'
         b'\treturn category in (ikashop.SHOP_SEARCH_CATEGORY_ARMOR, ikashop.SHOP_SEARCH_CATEGORY_WEAPON, ikashop.SHOP_SEARCH_CATEGORY_JEWELRY)\r\n'
         b'\r\n'
         b'def search_category(category, sub_category=-1, isSearchAttr=False, selectedItem=None):\r\n'
         b'\tprint category, sub_category\r\n'
         b'\tconstInfo.OFFLINESHOP_LAST_SEARCHED_CATEGORY = category\r\n'
         b'\tconstInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY = sub_category\r\n'),
        (b'\t\tsearch_data = search_data["sub"][sub_category]\r\n'
         b'\t\tsearchIndex += sub_category\r\n'
         b'\r\n'
         b'\tfor data in search_data["itemList"]:\r\n'
         b'\t\tconstInfo.OFFLINESHOP_LAST_SEARCHED_ITEMS.append(data)\r\n'
         b'\r\n'
         b'\tprint searchIndex\r\n'
         b'\tikashop.SendSearchItem(searchIndex, 0)\r\n'
         b'\r\n'
         b'def get_item_list(category, sub_category=-1):\r\n'
         b'\tdata = SHOP_SEARCH_FILTERS[category]\r\n',
         b'\t\tsearch_data = search_data["sub"][sub_category]\r\n'
         b'\t\tsearchIndex += sub_category\r\n'
         b'\r\n'
         b'\t# shop search: one item picked in the grid -> search only that item.\r\n'
         b'\t# The server reads it from the second argument as vnum * 1000 + socket0\r\n'
         b'\t# (0 = whole category).\r\n'
         b'\tsearchItemCode = 0\r\n'
         b'\tif selectedItem and selectedItem[1] >= 0 and selectedItem[1] < 1000:\r\n'
         b'\t\tsearchItemCode = selectedItem[0] * 1000 + selectedItem[1]\r\n'
         b'\t\tconstInfo.OFFLINESHOP_LAST_SEARCHED_ITEMS.append(selectedItem)\r\n'
         b'\telse:\r\n'
         b'\t\tfor data in search_data["itemList"]:\r\n'
         b'\t\t\tconstInfo.OFFLINESHOP_LAST_SEARCHED_ITEMS.append(data)\r\n'
         b'\r\n'
         b'\tprint searchIndex, searchItemCode\r\n'
         b'\tikashop.SendSearchItem(searchIndex, searchItemCode)\r\n'
         b'\r\n'
         b'def get_item_list(category, sub_category=-1):\r\n'
         b'\tdata = SHOP_SEARCH_FILTERS[category]\r\n'),
        (b'\t\tself.subCategory = -1\r\n'
         b'\t\tself.itemToolTip = None\r\n'
         b'\t\tself.isSearchAttr = False\r\n'
         b'\r\n'
         b'\t@ui.WindowDestroy\r\n'
         b'\tdef Destroy(self):\r\n',
         b'\t\tself.subCategory = -1\r\n'
         b'\t\tself.itemToolTip = None\r\n'
         b'\t\tself.isSearchAttr = False\r\n'
         b'\t\tself.selectedItemIndex = -1\r\n'
         b'\r\n'
         b'\t@ui.WindowDestroy\r\n'
         b'\tdef Destroy(self):\r\n'),
        (b'\t\t\tself.itemSlot.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)\r\n'
         b'\t\t\tself.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.__ShowToolTip))\r\n'
         b'\t\t\tself.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.__HideToolTip))\r\n'
         b'\r\n'
         b'\t\t\tself.searchButton.SAFE_SetEvent(self.__OnSearch)\r\n'
         b'\t\t\tself.clearButton.SAFE_SetEvent(self.__OnClear)\r\n',
         b'\t\t\tself.itemSlot.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)\r\n'
         b'\t\t\tself.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.__ShowToolTip))\r\n'
         b'\t\t\tself.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.__HideToolTip))\r\n'
         b'\t\t\tself.itemSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.__OnSelectItem))\r\n'
         b'\t\t\tself.itemSlot.SetUnselectItemSlotEvent(ui.__mem_func__(self.__OnSelectItem))\r\n'
         b'\r\n'
         b'\t\t\tself.searchButton.SAFE_SetEvent(self.__OnSearch)\r\n'
         b'\t\t\tself.clearButton.SAFE_SetEvent(self.__OnClear)\r\n'),
        (b'\t\t\t\tindex = y * self.itemSlot.x_width + x\r\n'
         b'\t\t\t\tself.itemSlot.SetItemSlot(index, 0, 0)\r\n'
         b'\t\t\t\tself.itemSlot.ClearSlot(index)\r\n'
         b'\r\n'
         b'\t\titemList = get_item_list(self.category, self.subCategory)\r\n'
         b'\t\tidx = 0\r\n',
         b'\t\t\t\tindex = y * self.itemSlot.x_width + x\r\n'
         b'\t\t\t\tself.itemSlot.SetItemSlot(index, 0, 0)\r\n'
         b'\t\t\t\tself.itemSlot.ClearSlot(index)\r\n'
         b'\t\t\t\tself.itemSlot.DeactivateSlot(index)\r\n'
         b'\r\n'
         b'\t\titemList = get_item_list(self.category, self.subCategory)\r\n'
         b'\t\tidx = 0\r\n'),
        (b'\t\t\tself.itemSlot.SetItemSlot(idx, itemData[0], 0, socket=(itemData[1], 0, 0))\r\n'
         b'\t\t\tidx += 1\r\n'
         b'\r\n'
         b'\tdef __CreateCategory(self, categoryIndex):\r\n'
         b'\t\tif categoryIndex == "break":\r\n'
         b'\t\t\tstep = ui.Window()\r\n',
         b'\t\t\tself.itemSlot.SetItemSlot(idx, itemData[0], 0, socket=(itemData[1], 0, 0))\r\n'
         b'\t\t\tidx += 1\r\n'
         b'\r\n'
         b'\t\tif self.selectedItemIndex >= len(itemList):\r\n'
         b'\t\t\tself.selectedItemIndex = -1\r\n'
         b'\t\tif self.selectedItemIndex >= 0:\r\n'
         b'\t\t\tself.itemSlot.ActivateSlot(self.selectedItemIndex)\r\n'
         b'\t\tself.itemSlot.RefreshSlot()\r\n'
         b'\r\n'
         b'\tdef __OnSelectItem(self, index):\r\n'
         b'\t\t# click an item = search only that item, click it again = whole category\r\n'
         b'\t\tif self.category < 0:\r\n'
         b'\t\t\treturn\r\n'
         b'\t\titemList = get_item_list(self.category, self.subCategory)\r\n'
         b'\t\tif index < 0 or index >= len(itemList):\r\n'
         b'\t\t\treturn\r\n'
         b'\t\tif self.selectedItemIndex == index:\r\n'
         b'\t\t\tself.selectedItemIndex = -1\r\n'
         b'\t\telse:\r\n'
         b'\t\t\tself.selectedItemIndex = index\r\n'
         b'\t\tself.__RefreshCategoryItems()\r\n'
         b'\r\n'
         b'\tdef __GetSelectedItem(self):\r\n'
         b'\t\tif self.selectedItemIndex < 0 or self.category < 0 or is_wearable_search(self.category):\r\n'
         b'\t\t\treturn None\r\n'
         b'\t\titemList = get_item_list(self.category, self.subCategory)\r\n'
         b'\t\tif self.selectedItemIndex >= len(itemList):\r\n'
         b'\t\t\treturn None\r\n'
         b'\t\treturn itemList[self.selectedItemIndex]\r\n'
         b'\r\n'
         b'\tdef __CreateCategory(self, categoryIndex):\r\n'
         b'\t\tif categoryIndex == "break":\r\n'
         b'\t\t\tstep = ui.Window()\r\n'),
        (b'\r\n'
         b'\t\tself.category = category\r\n'
         b'\t\tself.subCategory = -1\r\n'
         b'\t\tself.__RefreshSubCategoryButtons()\r\n'
         b'\t\tself.__RefreshCategory()\r\n'
         b'\t\tif category >= 0:\r\n',
         b'\r\n'
         b'\t\tself.category = category\r\n'
         b'\t\tself.subCategory = -1\r\n'
         b'\t\tself.selectedItemIndex = -1\r\n'
         b'\t\tself.__RefreshSubCategoryButtons()\r\n'
         b'\t\tself.__RefreshCategory()\r\n'
         b'\t\tif category >= 0:\r\n'),
        (b'\r\n'
         b'\tdef __OnClickSubCategory(self, index):\r\n'
         b'\t\tself.subCategory = index\r\n'
         b'\t\tself.__RefreshSubCategoryButtons()\r\n'
         b'\t\tself.__RefreshCategory()\r\n'
         b'\r\n',
         b'\r\n'
         b'\tdef __OnClickSubCategory(self, index):\r\n'
         b'\t\tself.subCategory = index\r\n'
         b'\t\tself.selectedItemIndex = -1\r\n'
         b'\t\tself.__RefreshSubCategoryButtons()\r\n'
         b'\t\tself.__RefreshCategory()\r\n'
         b'\r\n'),
        (b'\tdef __OnSearch(self):\r\n'
         b'\t\tif self.category >= 0:\r\n'
         b'\t\t\tself.__OnClear()\r\n'
         b'\t\t\tsearch_category(self.category, self.subCategory, self.isSearchAttr)\r\n'
         b'\r\n'
         b'\tdef __OnClear(self):\r\n'
         b'\t\tikashop.ClearFoundShopMap()\r\n',
         b'\tdef __OnSearch(self):\r\n'
         b'\t\tif self.category >= 0:\r\n'
         b'\t\t\tself.__OnClear()\r\n'
         b'\t\t\tsearch_category(self.category, self.subCategory, self.isSearchAttr, self.__GetSelectedItem())\r\n'
         b'\r\n'
         b'\tdef __OnClear(self):\r\n'
         b'\t\tikashop.ClearFoundShopMap()\r\n'),
    ],
    # The offline shop's edit grid removes an item on a left click and never
    # asked whether the slot held one: a click on an empty slot was a KeyError
    # in syserr.txt (slots 44, 45, 55 and 57 in l0st3k's, 15 September). An
    # empty slot does nothing now.
    'offlineshopmanage.py': [
        (b'\tdef RemoveItem(self, slotIndex):\r\n'
         b'\t\tikashop.SendRemoveItem(constInfo.myshop_data["items"][slotIndex]["id"])\r\n',
         b'\tdef RemoveItem(self, slotIndex):\r\n'
         b'\t\titemData = constInfo.myshop_data["items"].get(slotIndex)\r\n'
         b'\t\tif not itemData:\r\n'
         b'\t\t\treturn\r\n'
         b'\t\tikashop.SendRemoveItem(itemData["id"])\r\n'),
        # And "edit the price of similar items" (ctrl + right click) sent one
        # packet per line in a single frame, while the server takes one shop
        # action per 200 ms: the first line was repriced and every other one
        # answered "wait a moment" (uxietoszef, then Nagash with a screenshot,
        # 20 September). They leave through shoppricepump.py now, a quarter of
        # a second apart - the same shape as the inventory's auto-stack.
        (b'\t\t\t\tfor i in item_list:\r\n'
         b'\t\t\t\t\tself.__SendEditItemPricePacket(i, inputPrice)\r\n',
         b'\t\t\t\tshoppricepump.Queue([(i, inputPrice) for i in item_list])\r\n'),
        (b'import ikashop\r\n', b'import ikashop\r\nimport shoppricepump\r\n'),
    ],
    'uisystem.py': [
        # Numer wersji w oknie systemowym przez klucz locale (2.0.23).
        (b'\t\tversion_string = "Wersja: %d.%d.%d%s" % (\r\n',
         b'\t\tversion_string = uiScriptLocale.SYSTEM_VERSION % (\r\n'),
        (b'\t\tutils.open_url("https://mt2009.pl/Identity/Account/Manage/Support")\r\n',
         b'\t\tutils.open_url("https://discord.gg/pt5tvnrN6")\r\n'),
    ],
    'intrologin.py': [
        # The second channel (serverinfo.py lists two) is the server's to
        # switch on (M2_PLAYERBOT_CH2): a channel that does not answer is not
        # listed, CH1 always is, and a selection left on a hidden channel
        # falls back to CH1. The list is keyed by line, and CH1 is line 0.
        (b'\t\tfor channelID, channelDataDict in channelDict.items():\r\n'
         b'\t\t\tchannelName = channelDataDict["name"]\r\n'
         b'\t\t\tchannelState = channelDataDict["state"]\r\n'
         b'\t\t\tself.channelList.InsertItem(channelID, "%s %s" % (channelName, channelState))\r\n'
         b'\r\n'
         b'\t\tself.channelList.SelectItem(bakChannelID)\r\n',
         b'\t\tshown = []\r\n'
         b'\t\tfor channelID, channelDataDict in channelDict.items():\r\n'
         b'\t\t\tchannelName = channelDataDict["name"]\r\n'
         b'\t\t\tchannelState = channelDataDict["state"]\r\n'
         b'\t\t\t# The second channel runs only when the server switches it on\r\n'
         b'\t\t\t# (M2_PLAYERBOT_CH2): listed once it answers, never as a dead line.\r\n'
         b'\t\t\t# CH1 is always the first line, so a line is still its channel.\r\n'
         b'\t\t\tif channelID > 0 and channelState in (serverInfo.STATE_NONE, serverInfo.STATE_DICT[0]):\r\n'
         b'\t\t\t\tcontinue\r\n'
         b'\t\t\tself.channelList.InsertItem(channelID, "%s %s" % (channelName, channelState))\r\n'
         b'\t\t\tshown.append(channelID)\r\n'
         b'\r\n'
         b'\t\tif bakChannelID not in shown:\r\n'
         b'\t\t\tbakChannelID = 0\r\n'
         b'\t\tself.channelList.SelectItem(bakChannelID)\r\n'),
        (b'\t\tself.homePageButton.SAFE_SetEvent(self.OpenURL, "https://mt2009.pl/")\r\n',
         b'\t\tself.homePageButton.SAFE_SetEvent(self.OpenURL, "https://github.com/TieruYT/metin2-playerbots")\r\n'),
        (b'\t\tself.facebookButton.SAFE_SetEvent(self.OpenURL, "https://www.facebook.com/Metin2009PL")\r\n',
         b'\t\tself.facebookButton.SAFE_SetEvent(self.OpenURL, "https://buycoffee.to/metin2-playerbots")\r\n'),
        (b'\t\tself.discordButton.SAFE_SetEvent(self.OpenURL, "https://discord.gg/RhUaGRYZG7")\r\n',
         b'\t\tself.discordButton.SAFE_SetEvent(self.OpenURL, "https://discord.gg/pt5tvnrN6")\r\n'),
    ],
    # The game options get a "Tytuly botow" row under the floating text one:
    # a bot's personality title (playerbot_status_tail.py, 2.0.53) or the
    # classic alignment title, the player's own choice ("dodac w opcjach gry
    # aby moc przelaczac pomiedzy klasycznymi i aktualnymi", NerrVoVy, 15
    # September). The two buttons follow the night mode's pattern: bound by
    # name, refreshed from the module, and the click writes the choice through
    # SetTitlesEnabled. The dialog's script grows by one row's height.
    'uigameoption.py': [
        (b'\t\tself.RefreshFloatingTextButtons()\r\n'
         b'\r\n'
         b'\tdef __del__(self):\r\n',
         b'\t\tself.RefreshFloatingTextButtons()\r\n'
         b'\t\tself.RefreshBotTitleButtons()\r\n'
         b'\r\n'
         b'\tdef __del__(self):\r\n'),
        (b'\t\tself.floatingTextButtonList = []\r\n'
         b'\t\tself.toolTip = None\r\n',
         b'\t\tself.floatingTextButtonList = []\r\n'
         b'\t\tself.botTitleButtonList = []\r\n'
         b'\t\tself.toolTip = None\r\n'),
        (b'\t\t\tself.floatingTextButtonList.append(GetObject("floating_text_off"))\r\n'
         b'\r\n'
         b'\t\texcept:\r\n',
         b'\t\t\tself.floatingTextButtonList.append(GetObject("floating_text_off"))\r\n'
         b'\r\n'
         b'\t\t\tself.botTitleButtonList.append(GetObject("bot_title_personality_button"))\r\n'
         b'\t\t\tself.botTitleButtonList.append(GetObject("bot_title_classic_button"))\r\n'
         b'\r\n'
         b'\t\texcept:\r\n'),
        (b'\t\tself.floatingTextButtonList[2].SAFE_SetEvent(self.__OnClickFloatingTextButton, 0)\r\n',
         b'\t\tself.floatingTextButtonList[2].SAFE_SetEvent(self.__OnClickFloatingTextButton, 0)\r\n'
         b'\r\n'
         b'\t\tself.botTitleButtonList[0].SAFE_SetEvent(self.__OnClickBotTitleButton, 1)\r\n'
         b'\t\tself.botTitleButtonList[1].SAFE_SetEvent(self.__OnClickBotTitleButton, 0)\r\n'),
        (b'\tdef __OnClickFloatingTextButton(self, state):\r\n'
         b'\t\tsystemSetting.SetShowFloatingText(state)\r\n'
         b'\t\tself.RefreshFloatingTextButtons()\r\n'
         b'\r\n',
         b'\tdef __OnClickFloatingTextButton(self, state):\r\n'
         b'\t\tsystemSetting.SetShowFloatingText(state)\r\n'
         b'\t\tself.RefreshFloatingTextButtons()\r\n'
         b'\r\n'
         b'\t# Whether a bot\'s personality shows in its own row over its head\r\n'
         b'\t# (NerrVoVy, 15 September; that row moved off the ranga/title spot\r\n'
         b'\t# 2026-09-16 - see playerbot_status_tail.py). playerbot_titles.cfg keeps\r\n'
         b'\t# the choice; the classic alignment title (ranga) is unaffected either way.\r\n'
         b'\tdef __OnClickBotTitleButton(self, enabled):\r\n'
         b'\t\timport playerbot_status_tail\r\n'
         b'\t\tplayerbot_status_tail.SetTitlesEnabled(enabled)\r\n'
         b'\t\tself.RefreshBotTitleButtons()\r\n'
         b'\r\n'
         b'\tdef RefreshBotTitleButtons(self):\r\n'
         b'\t\timport playerbot_status_tail\r\n'
         b'\t\tfor btn in self.botTitleButtonList:\r\n'
         b'\t\t\tbtn.SetUp()\r\n'
         b'\t\tif playerbot_status_tail.TitlesEnabled():\r\n'
         b'\t\t\tself.botTitleButtonList[0].Down()\r\n'
         b'\t\telse:\r\n'
         b'\t\t\tself.botTitleButtonList[1].Down()\r\n'
         b'\r\n'),
    ],
    'uiscript/gameoptiondialog.py': [
        # Migracje napisow z naszych wlasnych wstawek na klucze locale (2.0.23).
        (b'\t\t\t\t\t"text" : "Tytu\\xb3y bot\\xf3w",\r\n',
         b'\t\t\t\t\t"text" : uiScriptLocale.BOT_TITLES_LABEL,\r\n', True),
        (b'\t\t\t\t\t"text" : "Osobowo\\x9c\\xe6",\r\n',
         b'\t\t\t\t\t"text" : uiScriptLocale.BOT_TITLES_PERSONALITY,\r\n', True),
        (b'\t\t\t\t\t"text" : "Wy\xb3\xb9czone",\r\n',
         b'\t\t\t\t\t"text" : uiScriptLocale.BOT_TITLES_OFF,\r\n', True),
        (b'\t"width" : 300,\r\n'
         b'\t"height" : 25*16+8,\r\n',
         b'\t"width" : 300,\r\n'
         b'\t"height" : 25*16+8+21,\r\n'),
        (b'\t\t\t"width" : 300,\r\n'
         b'\t\t\t"height" : 25*16+8,\r\n',
         b'\t\t\t"width" : 300,\r\n'
         b'\t\t\t"height" : 25*16+8+21,\r\n'),
        (b'\t\t\t\t\t"text" : uiScriptLocale.GAME_OPTIONS_FLOATING_TEXT_2,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"default_image" : ROOT_PATH + "middle_button_01.sub",\r\n'
         b'\t\t\t\t\t"over_image" : ROOT_PATH + "middle_button_02.sub",\r\n'
         b'\t\t\t\t\t"down_image" : ROOT_PATH + "middle_button_03.sub",\r\n'
         b'\t\t\t\t},\r\n'
         b'\t\t\t],\r\n',
         b'\t\t\t\t\t"text" : uiScriptLocale.GAME_OPTIONS_FLOATING_TEXT_2,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"default_image" : ROOT_PATH + "middle_button_01.sub",\r\n'
         b'\t\t\t\t\t"over_image" : ROOT_PATH + "middle_button_02.sub",\r\n'
         b'\t\t\t\t\t"down_image" : ROOT_PATH + "middle_button_03.sub",\r\n'
         b'\t\t\t\t},\r\n'
         b'\r\n'
         b'\t\t\t\t## BOT PERSONALITIES (playerbot_status_tail.py): shown in their own\r\n'
         b'\t\t\t\t## row over each bot\'s head, independent of the classic alignment\r\n'
         b'\t\t\t\t## title (ranga). The strings are CP1250 escapes so the file stays\r\n'
         b'\t\t\t\t## ASCII like the rest of the root.\r\n'
         b'\t\t\t\t{\r\n'
         b'\t\t\t\t\t"name" : "bot_title_text",\r\n'
         b'\t\t\t\t\t"type" : "text",\r\n'
         b'\r\n'
         b'\t\t\t\t\t"x" : LINE_LABEL_X,\r\n'
         b'\t\t\t\t\t"y" : 382+2,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"text" : uiScriptLocale.BOT_TITLES_LABEL,\r\n'
         b'\t\t\t\t},\r\n'
         b'\t\t\t\t{\r\n'
         b'\t\t\t\t\t"name" : "bot_title_personality_button",\r\n'
         b'\t\t\t\t\t"type" : "radio_button",\r\n'
         b'\r\n'
         b'\t\t\t\t\t"x" : LINE_DATA_X,\r\n'
         b'\t\t\t\t\t"y" : 382,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"text" : uiScriptLocale.BOT_TITLES_PERSONALITY,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"default_image" : ROOT_PATH + "middle_button_01.sub",\r\n'
         b'\t\t\t\t\t"over_image" : ROOT_PATH + "middle_button_02.sub",\r\n'
         b'\t\t\t\t\t"down_image" : ROOT_PATH + "middle_button_03.sub",\r\n'
         b'\t\t\t\t},\r\n'
         b'\t\t\t\t{\r\n'
         b'\t\t\t\t\t"name" : "bot_title_classic_button",\r\n'
         b'\t\t\t\t\t"type" : "radio_button",\r\n'
         b'\r\n'
         b'\t\t\t\t\t"x" : LINE_DATA_X+MIDDLE_BUTTON_WIDTH,\r\n'
         b'\t\t\t\t\t"y" : 382,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"text" : uiScriptLocale.BOT_TITLES_OFF,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"default_image" : ROOT_PATH + "middle_button_01.sub",\r\n'
         b'\t\t\t\t\t"over_image" : ROOT_PATH + "middle_button_02.sub",\r\n'
         b'\t\t\t\t\t"down_image" : ROOT_PATH + "middle_button_03.sub",\r\n'
         b'\t\t\t\t},\r\n'
         b'\t\t\t],\r\n'),
    ],
    # Four bag pages (playerbotify.py and clientify.py, four pages): the two
    # large tabs become four small ones, 32 pixels each, spread under the
    # 160-pixel grid that starts at x 8. The locale has tooltips for the first
    # two pages only, and a script under the loader's sandbox is no place for
    # getattr, so the new ones say it themselves.
    'uiscript/inventorywindow.py': [
        (b'\t\t\t\t\t"tooltip_text" : "3. Ekwipunek",\r\n',
         b'\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_PAGE_BUTTON_TOOLTIP_3,\r\n', True),
        (b'\t\t\t\t\t"tooltip_text" : "4. Ekwipunek",\r\n',
         b'\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_PAGE_BUTTON_TOOLTIP_4,\r\n', True),
        (b'\t\t\t\t\t\t\t"tooltip_text" : "Scal i uporz\\xb9dkuj",\r\n',
         b'\t\t\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_SORT_STACK,\r\n', True),
        (b'\t\t\t\t\t"x" : 10,\r\n'
         b'\t\t\t\t\t"y" : 33 + 191,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"default_image" : "d:/ymir work/ui/game/windows/tab_button_large_01.sub",\r\n'
         b'\t\t\t\t\t"over_image" : "d:/ymir work/ui/game/windows/tab_button_large_02.sub",\r\n'
         b'\t\t\t\t\t"down_image" : "d:/ymir work/ui/game/windows/tab_button_large_03.sub",\r\n'
         b'\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_PAGE_BUTTON_TOOLTIP_1,\r\n',
         b'\t\t\t\t\t"x" : 12,\r\n'
         b'\t\t\t\t\t"y" : 33 + 191,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"default_image" : "d:/ymir work/ui/game/windows/tab_button_small_01.sub",\r\n'
         b'\t\t\t\t\t"over_image" : "d:/ymir work/ui/game/windows/tab_button_small_02.sub",\r\n'
         b'\t\t\t\t\t"down_image" : "d:/ymir work/ui/game/windows/tab_button_small_03.sub",\r\n'
         b'\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_PAGE_BUTTON_TOOLTIP_1,\r\n'),
        (b'\t\t\t\t\t"x" : 10 + 78,\r\n'
         b'\t\t\t\t\t"y" : 33 + 191,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"default_image" : "d:/ymir work/ui/game/windows/tab_button_large_01.sub",\r\n'
         b'\t\t\t\t\t"over_image" : "d:/ymir work/ui/game/windows/tab_button_large_02.sub",\r\n'
         b'\t\t\t\t\t"down_image" : "d:/ymir work/ui/game/windows/tab_button_large_03.sub",\r\n'
         b'\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_PAGE_BUTTON_TOOLTIP_2,\r\n',
         b'\t\t\t\t\t"x" : 52,\r\n'
         b'\t\t\t\t\t"y" : 33 + 191,\r\n'
         b'\r\n'
         b'\t\t\t\t\t"default_image" : "d:/ymir work/ui/game/windows/tab_button_small_01.sub",\r\n'
         b'\t\t\t\t\t"over_image" : "d:/ymir work/ui/game/windows/tab_button_small_02.sub",\r\n'
         b'\t\t\t\t\t"down_image" : "d:/ymir work/ui/game/windows/tab_button_small_03.sub",\r\n'
         b'\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_PAGE_BUTTON_TOOLTIP_2,\r\n'),
        (b'\t\t\t\t\t\t\t"text" : "II",\r\n'
         b'\t\t\t\t\t\t},\r\n'
         b'\t\t\t\t\t),\r\n'
         b'\t\t\t\t},\r\n'
         b'\r\n'
         b'\t\t\t\t# {\r\n'
         b'\t\t\t\t# \t"name" : "Inventory_Tab_03",\r\n',
         b'\t\t\t\t\t\t\t"text" : "II",\r\n'
         b'\t\t\t\t\t\t},\r\n'
         b'\t\t\t\t\t),\r\n'
         b'\t\t\t\t},\r\n'
         + b''.join(
             b'\t\t\t\t{\r\n'
             b'\t\t\t\t\t"name" : "Inventory_Tab_%s",\r\n'
             b'\t\t\t\t\t"type" : "radio_button",\r\n'
             b'\r\n'
             b'\t\t\t\t\t"x" : %s,\r\n'
             b'\t\t\t\t\t"y" : 33 + 191,\r\n'
             b'\r\n'
             b'\t\t\t\t\t"default_image" : "d:/ymir work/ui/game/windows/tab_button_small_01.sub",\r\n'
             b'\t\t\t\t\t"over_image" : "d:/ymir work/ui/game/windows/tab_button_small_02.sub",\r\n'
             b'\t\t\t\t\t"down_image" : "d:/ymir work/ui/game/windows/tab_button_small_03.sub",\r\n'
             b'\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_PAGE_BUTTON_TOOLTIP_%s,\r\n'
             b'\r\n'
             b'\t\t\t\t\t"children" :\r\n'
             b'\t\t\t\t\t(\r\n'
             b'\t\t\t\t\t\t{\r\n'
             b'\t\t\t\t\t\t\t"name" : "Inventory_Tab_%s_Print",\r\n'
             b'\t\t\t\t\t\t\t"type" : "text",\r\n'
             b'\r\n'
             b'\t\t\t\t\t\t\t"x" : 0,\r\n'
             b'\t\t\t\t\t\t\t"y" : 0,\r\n'
             b'\r\n'
             b'\t\t\t\t\t\t\t"all_align" : "center",\r\n'
             b'\r\n'
             b'\t\t\t\t\t\t\t"text" : "%s",\r\n'
             b'\t\t\t\t\t\t},\r\n'
             b'\t\t\t\t\t),\r\n'
             b'\t\t\t\t},\r\n' % (num, x, page, num, label)
             for num, x, page, label in ((b'03', b'92', b'3', b'III'), (b'04', b'132', b'4', b'IV')))
         + b'\r\n'
         b'\t\t\t\t# {\r\n'
         b'\t\t\t\t# \t"name" : "Inventory_Tab_03",\r\n'),
        # The auto-stack button's tooltip: it pours and orders now. CP1250 as
        # an escape, like the options' row, so the script stays ASCII.
        (b'\t\t\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_AUTOSTACK,\r\n',
         b'\t\t\t\t\t\t\t"tooltip_text" : uiScriptLocale.INVENTORY_SORT_STACK,\r\n'),
    ],
    # The requirement counts under a locked horse-bag slot and a special
    # shop's price ("(0 na 60)") read two bag pages and, with the horse out,
    # the third - the horse page when the bag had two. With four pages that
    # left pages III and IV and the horse bag uncounted (blasty, 18
    # September). The server's own check (CountSpecifyItem) was right all
    # along; the window only said otherwise.
    'utils.py': [
        (b'\tpageCount = 2\r\n'
         b'\tif constInfo.IS_HORSE_SUMMONED:\r\n'
         b'\t\tpageCount += 1\r\n'
         b'\r\n'
         b'\tfor i in xrange(player.INVENTORY_PAGE_SIZE * pageCount):\r\n',
         b'\t# Every bag page, and the horse page while the horse is out.\r\n'
         b'\tslotCount = player.INVENTORY_DEFAULT_MAX_NUM\r\n'
         b'\tif constInfo.IS_HORSE_SUMMONED:\r\n'
         b'\t\tslotCount = player.INVENTORY_MAX_NUM\r\n'
         b'\r\n'
         b'\tfor i in xrange(slotCount):\r\n'),
    ],
    'uirefine.py': [
        # The server is in refine mode from the moment it sends the dialog and
        # refuses every move in the bag until the window answers. A window that
        # raised before Show() never answers: German, Spanish, Italian,
        # Portuguese, Romanian and Turkish players had REFINE_COST taking a
        # number where NumberToMoneyString hands a string, and every one of
        # them lost the bag at the blacksmith until the next login (23
        # September). The texts are fixed (localeify.py); this is so the next
        # bad text, or anything else in Open, costs a window and not a bag.
        (b'\tdef Open(self, targetItemPos, nextGradeItemVnum, cost, prob, type):\r\n'
         b'\r\n'
         b'\t\tif False == self.isLoaded:\r\n',
         b'\tdef Open(self, targetItemPos, nextGradeItemVnum, cost, prob, type):\r\n'
         b'\t\ttry:\r\n'
         b'\t\t\tself.__OpenWindow(targetItemPos, nextGradeItemVnum, cost, prob, type)\r\n'
         b'\t\texcept:\r\n'
         b'\t\t\t# The cancel is the answer the server waits for; the error\r\n'
         b'\t\t\t# still goes to syserr.txt.\r\n'
         b'\t\t\tnet.SendRefinePacket(255, 255)\r\n'
         b'\t\t\tself.Hide()\r\n'
         b'\t\t\traise\r\n'
         b'\r\n'
         b'\tdef __OpenWindow(self, targetItemPos, nextGradeItemVnum, cost, prob, type):\r\n'
         b'\r\n'
         b'\t\tif False == self.isLoaded:\r\n'),
        (b'\t\tself.probText.SetText(localeInfo.REFINE_SUCCESS_PROBALITY % (self.percentage))\r\n'
         b'\t\tself.costText.SetText(localeInfo.REFINE_COST % localeInfo.NumberToMoneyString(self.cost))\r\n',
         b'\t\ttry:\r\n'
         b'\t\t\tself.probText.SetText(localeInfo.REFINE_SUCCESS_PROBALITY % (self.percentage))\r\n'
         b'\t\texcept:\r\n'
         b'\t\t\tself.probText.SetText("%d%%" % (self.percentage))\r\n'
         b'\t\ttry:\r\n'
         b'\t\t\tself.costText.SetText(localeInfo.REFINE_COST % localeInfo.NumberToMoneyString(self.cost))\r\n'
         b'\t\texcept:\r\n'
         b'\t\t\tself.costText.SetText(localeInfo.NumberToMoneyString(self.cost))\r\n'),
    ],
    'constinfo.py': [
        (b'\t"major" : 0,\r\n\t"minor" : 15,\r\n',
         b'\t"major" : 1,\r\n\t"minor" : 0,\r\n'),
    ],
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', required=True, help='directory holding the extracted stock root')
    args = parser.parse_args()
    os.makedirs(OUT, exist_ok=True)
    for name, edits in EDITS.items():
        src = os.path.join(args.root, name)
        if not os.path.isfile(src):
            raise SystemExit('clientrootify: no %s in %s' % (name, args.root))
        data = io.open(src, 'rb').read()
        for edit in edits:
            old, new = edit[0], edit[1]
            # A third element marks the pair optional: it is a migration of
            # text one of our own edits put there in an earlier version, so
            # neither side is in a pristine stock root and both are absent
            # once it has been done. Anything not marked stays compulsory.
            optional = len(edit) > 2 and edit[2]
            # Already ours, asked first: an edit that inserts after its anchor
            # keeps the anchor in its result, and asking for the anchor first
            # put the options' title methods into uigameoption.py twice each
            # time a published root was the base.
            if data.count(new) == 1:
                continue
            if optional and data.count(old) == 0:
                continue
            if data.count(old) != 1:
                raise SystemExit('clientrootify: %s: expected exactly one %r, found %d' % (name, old[:50], data.count(old)))
            data = data.replace(old, new)
        out = os.path.join(OUT, name)
        os.makedirs(os.path.dirname(out), exist_ok=True)  # uiscript/...
        io.open(out, 'wb').write(data)
        print('clientrootify: client-root/%s (%d bytes)' % (name, len(data)))


if __name__ == '__main__':
    main()
