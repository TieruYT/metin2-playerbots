import os
import app
import dbg
import grp
import item
import background
import chr
import chrmgr
import player
import snd
import chat
import textTail
import snd
import net
import effect
import wndMgr
import fly
import systemSetting
import quest
import guild
import skill
import messenger
import localeInfo
import constInfo
import exchange
import ime

import special_flags
import ui
import uiCommon
import uiPhaseCurtain
import uiMapNameShower
import uiPlayerGauge
import uiCharacter
import uiTarget
import uiExchange

import eventManager
import uiFishing

# PRIVATE_SHOP_PRICE_LIST
import uiPrivateShopBuilder
# END_OF_PRIVATE_SHOP_PRICE_LIST

import mouseModule
import consoleModule
import localeInfo

import playerSettingModule
import interfaceModule

import musicInfo
import debugInfo
import stringCommander

import offlineShopBuilder
from _weakref import proxy

import updateable
import uiAffectBar
import uiItemShop

import datetime
import uiFloatingText

import tween
import tweenCamera

import shop
import uiReputation
import time

import captcha
import uiGameMaster

if app.ENABLE_CINEMACHINE:
	import cinemachine

# SCREENSHOT_CWDSAVE
SCREENSHOT_CWDSAVE = True
SCREENSHOT_DIR = None

cameraDistance = 1550.0
cameraPitch = 27.0
cameraRotation = 0.0
cameraHeight = 100.0

FIXED_TIMESTEP_UPDATE = 167 # (0.0167f) assuming game is running in 60fps

testAlignment = 0

class GameWindow(ui.ScriptWindow):
	def __init__(self, stream):
		ui.ScriptWindow.__init__(self, "GAME")
		self.SetWindowName("game")
		net.SetPhaseWindow(net.PHASE_WINDOW_GAME, self)
		player.SetGameWindow(self)

		# Panel GM: constInfo.IsGM to zwykla flaga modulu - nie resetuje sie
		# sama miedzy postaciami w tej samej sesji klienta. Zerowana tu, przy
		# kazdym wejsciu do gry; serwer nadaje ja od nowa ("SetGMFlag") tylko
		# postaciom, ktore naprawde maja gm_level > 0.
		constInfo.IsGM = False

		# Znaczek Top1 (serwer: top1_badge_event, playerbot_manager.cpp): postac z
		# najwyzszym poziomem dostaje nieuzywana flage afektu AFF_HAIR (serwer nr 40,
		# czyli bit 39). Klient numeruje flagi od zera (AFFECT_YMIR=0 ... FIRE=28,
		# stad "28" dla AFF_FIRE=29 w starym uiAffectShower), wiec bit 39 = HAIR.
		# UWAGA: chr.AFFECT_CHINA_FIREWORK w tym buildzie zwraca 224 - to NIE jest
		# numer bitu, dlatego stala jest wpisana wprost. Tu doczepiamy do niej efekt
		# TOP1.mse - ten sam mechanizm, ktorym silnik rysuje znak GM (gm.mse dla
		# AFFECT_YMIR), wiec rozmiar i miganie sa identyczne.
		TOP1_AFFECT_BIT = 39
		try:
			# Zaczepienie na korzeniu postaci (""): przesuniecie z top1.mse jest wtedy
			# pionowe (z=110 wypadalo w pasie, z=240 = tuz nad nazwa). Na kosci glowy
			# ("Bip01 Head") obraca sie z kostka i laduje z boku postaci.
			chrmgr.RegisterEffect(chrmgr.EFFECT_AFFECT + TOP1_AFFECT_BIT, "", "d:/ymir work/effect/gm/top1.mse")
			dbg.TraceError("Top1 effect registered for affect bit %d (EFFECT_AFFECT=%s, POISON=%s, STUN=%s, MOV_SPEED=%s, FISH_MIND=%s)" % (
				TOP1_AFFECT_BIT, getattr(chrmgr, "EFFECT_AFFECT", "?"), getattr(chr, "AFFECT_POISON", "?"),
				getattr(chr, "AFFECT_STUN", "?"), getattr(chr, "AFFECT_MOV_SPEED_POTION", "?"), getattr(chr, "AFFECT_FISH_MIND", "?")))
		except Exception, e:
			dbg.TraceError("Top1 effect register failed: %s" % e)

		self.quickSlotPageIndex = 0
		self.lastPKModeSendedTime = 0
		self.pressNumber = None

		self.oneSecondTimer = 0

		self.guildWarQuestionDialog = None
		self.interface = None
		self.targetBoard = None
		self.console = None
		self.mapNameShower = None
		self.affectShower = None
		self.playerGauge = None

		self.stream=stream
		self.interface = interfaceModule.Interface()
		self.interface.MakeInterface()
		self.interface.ShowDefaultWindows()

		self.is_interface_hidden = False
		self.is_auto_night = False
		self.is_night_environment = False

		self.floating_texts = []
		self.updateable = []

		self.curtain = uiPhaseCurtain.PhaseCurtain()
		self.curtain.speed = 0.03
		self.curtain.Hide()

		self.targetBoard = uiTarget.TargetBoard()
		self.targetBoard.SetWhisperEvent(ui.__mem_func__(self.interface.OpenWhisperDialog))
		self.targetBoard.SetGMCheckEvent(ui.__mem_func__(self.interface.OpenGMLookupFor))
		self.targetBoard.SetEQEvent(ui.__mem_func__(self.interface.OpenGMEquipFor))
		self.targetBoard.SetToolTip(self.interface.tooltip)
		self.targetBoard.Hide()

		# Panel GM: /gmpanel_check_gm wyslane od razu w __init__ rozlaczalo
		# klienta (za wczesnie w handshake Select->Game) - odkladamy o kilkaset
		# klatek, patrz OnUpdate.
		self.__gmCheckSent = False
		self.__gmCheckFrames = 0

		self.console = consoleModule.ConsoleWindow()
		self.console.BindGameClass(self)
		self.console.SetConsoleSize(wndMgr.GetScreenWidth(), 200)
		self.console.Hide()

		self.mapNameShower = uiMapNameShower.MapNameShower()
		self.affectBar = uiAffectBar.AffectShower()

		self.playerGauge = uiPlayerGauge.PlayerGauge(self)
		self.playerGauge.Hide()

		self.tweenMgr = tween.TweenManager()

		if app.ENABLE_CINEMACHINE:
			self.cinemachineWindow = cinemachine.CinemachineWindow()
			self.cinemachineWindow.Hide()

		self.itemDropQuestionDialog = None

		self.__SetQuickSlotMode()

		self.__ServerCommand_Build()
		self.__ProcessPreservedServerCommand()

		self.partyRequestQuestionDialog = None
		self.partyInviteQuestionDialog = None

	def __del__(self):
		player.SetGameWindow(0)
		net.ClearPhaseWindow(net.PHASE_WINDOW_GAME, self)
		ui.ScriptWindow.__del__(self)

	def Open(self):
		app.SetFrameSkip(1)

		self.SetSize(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())

		self.quickSlotPageIndex = 0
		self.PickingCharacterIndex = -1
		self.PickingItemIndex = -1
		self.consoleEnable = False
		self.isShowDebugInfo = False
		self.ShowNameFlag = False

		# Panel GM: flaga GM-a obowiazuje na jedno wejscie do gry. __init__
		# wykonuje sie raz na uruchomienie klienta, wiec przy kolejnym
		# logowaniu w tej samej sesji pytanie /gmpanel_check_gm juz nie
		# szlo, a jednorazowy push serwera ("SetGMFlag", 3 s po wczytaniu
		# postaci) potrafi trafic jeszcze w ladowanie mapy i przepasc.
		# Wtedy przyciski GM na oknie celu sie nie pojawialy.
		constInfo.IsGM = False
		self.__gmCheckSent = False
		self.__gmCheckFrames = 0

		if app.ENABLE_CINEMACHINE:
			self.isCinemaMode = False;
			self.freeCameraSpeed = 1
			self.cameraFov = 30

		self.enableXMasBoom = False
		self.startTimeXMasBoom = 0.0
		self.indexXMasBoom = 0

		self.CreateUpdateables()

		global cameraDistance, cameraPitch, cameraRotation, cameraHeight

		app.SetCamera(cameraDistance, cameraPitch, cameraRotation, cameraHeight)

		constInfo.SET_DEFAULT_CAMERA_MAX_DISTANCE()
		constInfo.SET_DEFAULT_CHRNAME_COLOR()
		constInfo.SET_DEFAULT_FOG_LEVEL()
		constInfo.SET_DEFAULT_CONVERT_EMPIRE_LANGUAGE_ENABLE()
		constInfo.SET_DEFAULT_USE_ITEM_WEAPON_TABLE_ATTACK_BONUS()
		constInfo.SET_DEFAULT_USE_SKILL_EFFECT_ENABLE()

		# TWO_HANDED_WEAPON_ATTACK_SPEED_UP
		constInfo.SET_TWO_HANDED_WEAPON_ATT_SPEED_DECREASE_VALUE()
		# END_OF_TWO_HANDED_WEAPON_ATTACK_SPEED_UP

		import event
		event.SetLeftTimeString(localeInfo.UI_LEFT_TIME)

		textTail.EnablePKTitle(constInfo.PVPMODE_ENABLE)

		if constInfo.PVPMODE_TEST_ENABLE:
			self.testPKMode = ui.TextLine()
			self.testPKMode.SetPosition(0, 15)
			self.testPKMode.SetWindowHorizontalAlignCenter()
			self.testPKMode.SetHorizontalAlignCenter()
			self.testPKMode.SetFeather()
			self.testPKMode.SetOutline()
			self.testPKMode.Show()

			self.testAlignment = ui.TextLine()
			self.testAlignment.SetPosition(0, 35)
			self.testAlignment.SetWindowHorizontalAlignCenter()
			self.testAlignment.SetHorizontalAlignCenter()
			self.testAlignment.SetFeather()
			self.testAlignment.SetOutline()
			self.testAlignment.Show()

		self.__BuildKeyDict()
		self.__BuildDebugInfo()

		# PRIVATE_SHOP_PRICE_LIST
		uiPrivateShopBuilder.Clear()
		# END_OF_PRIVATE_SHOP_PRICE_LIST

		# UNKNOWN_UPDATE
		exchange.InitTrading()
		# END_OF_UNKNOWN_UPDATE


		## Sound
		snd.SetMusicVolume(systemSetting.GetMusicVolume()*net.GetFieldMusicVolume())
		snd.SetSoundVolume(systemSetting.GetSoundVolume())

		netFieldMusicFileName = net.GetFieldMusicFileName()
		if netFieldMusicFileName:
			snd.FadeInMusic("BGM/" + netFieldMusicFileName)
		elif musicInfo.fieldMusic != "":
			snd.FadeInMusic("BGM/" + musicInfo.fieldMusic)

		self.__SetQuickSlotMode()
		self.__SelectQuickPage(self.quickSlotPageIndex)

		self.SetFocus()
		self.Show()
		app.ShowCursor()

		net.SendEnterGamePacket()

		# START_GAME_ERROR_EXIT
		try:
			self.StartGame()
		except:
			import exception
			exception.Abort("GameWindow.Open")
		# END_OF_START_GAME_ERROR_EXIT

		# ex) cubeInformation[20383] = [ {"rewordVNUM": 72723, "rewordCount": 1, "materialInfo": "101,1&102,2", "price": 999 }, ... ]
		self.cubeInformation = {}
		self.currentCubeNPC = 0

		eventManager.EventManager().add_observer(eventManager.CHANGE_NIGHT_MODE, self.__OnChangeNightMode)
		self.__RefreshNightMode()
		eventManager.EventManager().add_observer(eventManager.ADD_FLOATING_TEXT, self.__AddFloatingText)

	def Close(self):
		self.Hide()

		self.oneSecondTimer = 0

		global cameraDistance, cameraPitch, cameraRotation, cameraHeight
		(cameraDistance, cameraPitch, cameraRotation, cameraHeight) = app.GetCamera()

		if musicInfo.fieldMusic != "":
			snd.FadeOutMusic("BGM/"+ musicInfo.fieldMusic)

		self.onPressKeyDict = None
		self.onClickKeyDict = None

		chat.Close()
		snd.StopAllSound()
		grp.InitScreenEffect()
		chr.Destroy()
		textTail.Clear()
		quest.Clear()
		background.Destroy()
		guild.Destroy()
		messenger.Destroy()
		skill.ClearSkillData()
		wndMgr.Unlock()
		mouseModule.mouseController.DeattachObject()

		self.DestroyUpdateables()
		self.floating_texts = []

		if app.ENABLE_CINEMACHINE:
			self.cinemachineWindow.Destroy()
			self.cinemachineWindow = None

		self.tweenMgr.Destroy()

		if self.guildWarQuestionDialog:
			self.guildWarQuestionDialog.Close()


		constInfo.OFFLINE_SHOP_SEARCH_CLEAR()
		constInfo.IS_HORSE_SUMMONED = False

		self.guildNameBoard = None
		self.partyRequestQuestionDialog = None
		self.partyInviteQuestionDialog = None
		self.guildInviteQuestionDialog = None
		self.guildWarQuestionDialog = None
		self.messengerAddFriendQuestion = None

		# UNKNOWN_UPDATE
		self.itemDropQuestionDialog = None
		# END_OF_UNKNOWN_UPDATE

		# QUEST_CONFIRM
		self.confirmDialog = None
		# END_OF_QUEST_CONFIRM

		self.PrintCoord = None
		self.FrameRate = None
		self.Pitch = None
		self.Splat = None
		self.TextureNum = None
		self.ObjectNum = None
		self.ViewDistance = None
		self.PrintMousePos = None

		self.ClearDictionary()

		self.playerGauge = None
		self.mapNameShower = None

		if self.affectBar:
			self.affectBar.Destroy()
			self.affectBar = None

		if self.console:
			self.console.BindGameClass(0)
			self.console.Close()
			self.console=None

		if self.targetBoard:
			self.targetBoard.Destroy()
			self.targetBoard = None

		if self.interface:
			self.interface.HideAllWindows()
			self.interface.Close()
			self.interface=None

		player.ClearSkillDict()
		player.ResetCameraRotation()

		constInfo.SPECIAL_FLAG = {}

		eventManager.EventManager().unregister_all_events()

		self.KillFocus()
		app.HideCursor()

		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

		print("---------------------------------------------------------------------------- CLOSE GAME WINDOW")

	def CreateUpdateables(self):
		self.updateable = []
		self.RegisterUpdatable(updateable.PickUpOnDownKey())
		import uiautohunt
		self.RegisterUpdatable(uiautohunt.GetHunter())

	def __AddFloatingText(self, text, is_negative, life_time=1.5):
		if systemSetting.GetShowFloatingText():
			print "__AddFloatingText", is_negative
			item = uiFloatingText.FloatingText(text, is_negative, life_time)
			self.floating_texts.append(item)

	def DestroyUpdateables(self):
		for i in self.updateable:
			i.Destroy()
			del i

		self.updateable = []

	def RegisterUpdatable(self, updateableObject):
		self.updateable.append(updateableObject)

	if app.ENABLE_CINEMACHINE:
		def __SetFreeCamera(self):
			self.isCinemaMode = not self.isCinemaMode
			app.FreeCamera()

			if self.isCinemaMode:
				chat.AppendChat(chat.CHAT_TYPE_INFO, "Cinematic mode ON!")
			else:
				chat.AppendChat(chat.CHAT_TYPE_INFO, "Cinematic mode OFF!")

		def __AddCameraFov(self):
			if not self.isCinemaMode:
				return

			self.cameraFov += 2
			app.SetFov(self.cameraFov)

		def __SubtractCameraFov(self):
			if not self.isCinemaMode:
				return

			self.cameraFov -= 2
			app.SetFov(self.cameraFov)

		def __AddFreeCameraSpeed(self, speed=0.05):
			if not self.isCinemaMode:
				return

			self.freeCameraSpeed += speed
			app.FreeCameraSpeed(self.freeCameraSpeed)

		def __SubtractFreeCameraSpeed(self, speed=0.05):
			if not self.isCinemaMode:
				return

			self.freeCameraSpeed -= speed
			app.FreeCameraSpeed(self.freeCameraSpeed)

		def CreateBezierPath(self, valueKey, num_points):
			import mathUtils

			waypoints = []
			n = len(constInfo.CAMERA_WAYPOINTS)
			for i in range(n):
				waypoints.append(constInfo.CAMERA_WAYPOINTS[i][valueKey])

			for i in range(num_points):
				t = i / float(num_points - 1)
				point = 0
				if valueKey == "rotation":
					point = mathUtils.bezier_rotation(t, *waypoints)
				else:
					point = mathUtils.bezier(t, *waypoints)
				if not constInfo.CAMERA_BEZIER_POINTS.has_key(valueKey):
					constInfo.CAMERA_BEZIER_POINTS[valueKey] = {}
				constInfo.CAMERA_BEZIER_POINTS[valueKey][i] = point

		def TestFreeCamera(self, num):
			(distance, pitch, rotation, height) = app.GetCamera()
			(x, y, z) = app.GetCameraPosition()

			if num == 1: # add waypoint
				(distance, pitch, rotation, height) = app.GetCamera()
				(x, y, z) = app.GetCameraPosition()

				waypointIndex = len(constInfo.CAMERA_WAYPOINTS)
				constInfo.CAMERA_WAYPOINTS[waypointIndex] = {
					"x" : x,
					"y" : y,
					"z" : z,
					"distance" : distance,
					"pitch" : pitch,
					"rotation" : rotation,
					"fov" : self.cameraFov,
				}

				chat.AppendChat(chat.CHAT_TYPE_INFO, "Added camera waypoint (%d) x: %d, y: %d, z: %d, distance: %d, pitch: %d, rotation: %d" %
								(waypointIndex, x, y, z, distance, pitch, rotation))

			elif num == 2:  # reset waypoints
				constInfo.CAMERA_WAYPOINTS = {}
				chat.AppendChat(chat.CHAT_TYPE_INFO, "Reset camera waypoints.")

			elif num == 3: # play waypoints bezier
				if len(constInfo.CAMERA_WAYPOINTS) < 1:
					return

				cameraTween = None
				if self.cinemachineWindow.cameraInterpolationMode == self.cinemachineWindow.MODE_BEZIER:
					num_points = 20

					isBezierRotation = False
					constInfo.CAMERA_BEZIER_POINTS = {}
					self.CreateBezierPath("x", num_points)
					self.CreateBezierPath("y", num_points)
					self.CreateBezierPath("z", num_points)
					self.CreateBezierPath("distance", num_points)
					self.CreateBezierPath("pitch", num_points)
					if self.cinemachineWindow.rotationInterpolationMode == self.cinemachineWindow.ROTATION_MODE_BEZIER:
						self.CreateBezierPath("rotation", num_points)
						isBezierRotation = True
					self.CreateBezierPath("fov", num_points)

					cameraTween = tweenCamera.CameraBezierTween(self.cinemachineWindow.TWEEN_DURATION, num_points, isBezierRotation)
				else:
					cameraTween = tweenCamera.CameraLerpTween(self.cinemachineWindow.TWEEN_DURATION)

				self.tweenMgr.CreateTween(cameraTween)
				cameraTween.StartTween()

			elif num == 4:
				self.cinemachineWindow.Toggle()

	def __PressF5Key(self):
		self.interface.offlineShopSearch.Open()
		self.interface.GetInterfaceWindow("GlobalRankingsManager").Open()

	def __BuildKeyDict(self):
		onPressKeyDict = {}


		onPressKeyDict[app.DIK_1]	= lambda : self.__PressNumKey(1)
		onPressKeyDict[app.DIK_2]	= lambda : self.__PressNumKey(2)
		onPressKeyDict[app.DIK_3]	= lambda : self.__PressNumKey(3)
		onPressKeyDict[app.DIK_4]	= lambda : self.__PressNumKey(4)
		onPressKeyDict[app.DIK_5]	= lambda : self.__PressNumKey(5)
		onPressKeyDict[app.DIK_6]	= lambda : self.__PressNumKey(6)
		onPressKeyDict[app.DIK_7]	= lambda : self.__PressNumKey(7)
		onPressKeyDict[app.DIK_8]	= lambda : self.__PressNumKey(8)
		onPressKeyDict[app.DIK_9]	= lambda : self.__PressNumKey(9)
		onPressKeyDict[app.DIK_F1]	= lambda : self.__PressQuickSlot(6)
		onPressKeyDict[app.DIK_F2]	= lambda : self.__PressQuickSlot(7)
		onPressKeyDict[app.DIK_F3]	= lambda : self.__PressQuickSlot(8)
		onPressKeyDict[app.DIK_F4]	= lambda : self.__PressQuickSlot(9)

		if app.ENABLE_CINEMACHINE:
			# onPressKeyDict[app.DIK_F5] = lambda: self.interface.potionRechargeDialog.Open()
			onPressKeyDict[app.DIK_F5] = lambda: self.__PressF5Key()
			onPressKeyDict[app.DIK_F6]	= lambda : self.__AddFreeCameraSpeed()
			onPressKeyDict[app.DIK_F7] = lambda: self.__SubtractFreeCameraSpeed()
			onPressKeyDict[app.DIK_F8] = lambda: self.__AddCameraFov()
			onPressKeyDict[app.DIK_F9] = lambda: self.__SubtractCameraFov()

		onPressKeyDict[app.DIK_LALT]		= lambda : self.ShowName()
		onPressKeyDict[app.DIK_LCONTROL]	= lambda : self.ShowMouseImage()
		onPressKeyDict[app.DIK_SYSRQ]		= lambda : self.SaveScreen()
		onPressKeyDict[app.DIK_SPACE]		= lambda : self.StartAttack()

		onPressKeyDict[app.DIK_UP]			= lambda : self.MoveUp()
		onPressKeyDict[app.DIK_DOWN]		= lambda : self.MoveDown()
		onPressKeyDict[app.DIK_LEFT]		= lambda : self.MoveLeft()
		onPressKeyDict[app.DIK_RIGHT]		= lambda : self.MoveRight()
		onPressKeyDict[app.DIK_W]			= lambda : self.MoveUp()
		onPressKeyDict[app.DIK_S]			= lambda : self.MoveDown()
		onPressKeyDict[app.DIK_A]			= lambda : self.MoveLeft()
		onPressKeyDict[app.DIK_D]			= lambda : self.MoveRight()

		onPressKeyDict[app.DIK_E]			= lambda: app.RotateCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_R]			= lambda: app.ZoomCamera(app.CAMERA_TO_NEGATIVE)
		#onPressKeyDict[app.DIK_F]			= lambda: app.ZoomCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_T]			= lambda: app.PitchCamera(app.CAMERA_TO_NEGATIVE)
		onPressKeyDict[app.DIK_G]			= self.__PressGKey
		onPressKeyDict[app.DIK_Q]			= self.__PressQKey

		onPressKeyDict[app.DIK_NUMPAD9]		= lambda: app.MovieResetCamera()
		onPressKeyDict[app.DIK_NUMPAD4]		= lambda: app.MovieRotateCamera(app.CAMERA_TO_NEGATIVE)
		onPressKeyDict[app.DIK_NUMPAD6]		= lambda: app.MovieRotateCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_PGUP]		= lambda: app.MovieZoomCamera(app.CAMERA_TO_NEGATIVE)
		onPressKeyDict[app.DIK_PGDN]		= lambda: app.MovieZoomCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_NUMPAD8]		= lambda: app.MoviePitchCamera(app.CAMERA_TO_NEGATIVE)
		onPressKeyDict[app.DIK_NUMPAD2]		= lambda: app.MoviePitchCamera(app.CAMERA_TO_POSITIVE)
		onPressKeyDict[app.DIK_GRAVE]		= lambda : self.PickUpNearbyItems()
		onPressKeyDict[app.DIK_Z]			= lambda : self.PickUpItem()
		onPressKeyDict[app.DIK_C]			= lambda state = "STATUS": self.interface.ToggleCharacterWindow(state)
		onPressKeyDict[app.DIK_V]			= lambda state = "SKILL": self.interface.ToggleCharacterWindow(state)
		#onPressKeyDict[app.DIK_B]			= lambda state = "EMOTICON": self.interface.ToggleCharacterWindow(state)
		onPressKeyDict[app.DIK_N]			= lambda state = "QUEST": self.interface.ToggleCharacterWindow(state)
		onPressKeyDict[app.DIK_I]			= lambda : self.interface.ToggleInventoryWindow()
		onPressKeyDict[app.DIK_O]			= lambda : self.interface.ToggleDragonSoulWindowWithNoInfo()
		onPressKeyDict[app.DIK_M]			= lambda : self.interface.PressMKey()
		#onPressKeyDict[app.DIK_H]			= lambda : self.interface.OpenHelpWindow()
		onPressKeyDict[app.DIK_ADD]			= lambda : self.interface.MiniMapScaleUp()
		onPressKeyDict[app.DIK_SUBTRACT]	= lambda : self.interface.MiniMapScaleDown()
		onPressKeyDict[app.DIK_L]			= lambda : self.interface.ToggleChatLogWindow()
		# Panel GM (F9) i admin botow (F10): kazde nacisniecie pyta serwer, ktory
		# sprawdza gm_level (cmd.cpp) i dopiero odsyla OpenGMPanelWindow /
		# OpenPlayerbotAdminWindow - zwyklemu graczowi nic sie nie otworzy.
		onPressKeyDict[app.DIK_F9]			= lambda : net.SendChatPacket("/gmpanel_open")
		onPressKeyDict[app.DIK_F10]			= lambda : net.SendChatPacket("/botadmin")
		onPressKeyDict[app.DIK_COMMA]		= lambda : self.ShowConsole()		# "`" key
		onPressKeyDict[app.DIK_LSHIFT]		= lambda : self.__ToggleSprint()

		onPressKeyDict[app.DIK_TAB]			 = self.__PressTABKey

		onPressKeyDict[app.DIK_J]			= lambda : self.__PressJKey()
		onPressKeyDict[app.DIK_H]			= lambda : self.__PressHKey()
		onPressKeyDict[app.DIK_B]			= lambda : self.__PressBKey()
		onPressKeyDict[app.DIK_F]			= lambda : self.__PressFKey()
		onPressKeyDict[app.DIK_Y] 			= lambda : self.interface.wndPlayerStat.Open()
		onPressKeyDict[app.DIK_P]			= lambda : self.__ToggleSidekick()
		# if app.ENABLE_IKASHOP_RENEWAL:
		# 	onPressKeyDict[app.DIK_F8]		= lambda : self.__PressF8Key()





		# CUBE_TEST
		#onPressKeyDict[app.DIK_K]			= lambda : self.interface.OpenCubeWindow()
		onPressKeyDict[app.DIK_K]			= lambda : self.__ToggleAutoHunt()
		# CUBE_TEST_END

		self.onPressKeyDict = onPressKeyDict

		onClickKeyDict = {}
		onClickKeyDict[app.DIK_UP] = lambda : self.StopUp()
		onClickKeyDict[app.DIK_DOWN] = lambda : self.StopDown()
		onClickKeyDict[app.DIK_LEFT] = lambda : self.StopLeft()
		onClickKeyDict[app.DIK_RIGHT] = lambda : self.StopRight()
		onClickKeyDict[app.DIK_SPACE] = lambda : self.EndAttack()

		onClickKeyDict[app.DIK_W] = lambda : self.StopUp()
		onClickKeyDict[app.DIK_S] = lambda : self.StopDown()
		onClickKeyDict[app.DIK_A] = lambda : self.StopLeft()
		onClickKeyDict[app.DIK_D] = lambda : self.StopRight()
		onClickKeyDict[app.DIK_Q] = lambda: app.RotateCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_E] = lambda: app.RotateCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_R] = lambda: app.ZoomCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_F] = lambda: app.ZoomCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_T] = lambda: app.PitchCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_G] = lambda: self.__ReleaseGKey()
		onClickKeyDict[app.DIK_NUMPAD4] = lambda: app.MovieRotateCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_NUMPAD6] = lambda: app.MovieRotateCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_PGUP] = lambda: app.MovieZoomCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_PGDN] = lambda: app.MovieZoomCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_NUMPAD8] = lambda: app.MoviePitchCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_NUMPAD2] = lambda: app.MoviePitchCamera(app.CAMERA_STOP)
		onClickKeyDict[app.DIK_LALT] = lambda: self.HideName()
		onClickKeyDict[app.DIK_LCONTROL] = lambda: self.HideMouseImage()
		onClickKeyDict[app.DIK_LSHIFT] = lambda: self.__SetQuickSlotMode()

		#if constInfo.PVPMODE_ACCELKEY_ENABLE:
		#	onClickKeyDict[app.DIK_B] = lambda: self.ChangePKMode()

		self.onClickKeyDict=onClickKeyDict
	# if app.ENABLE_IKASHOP_RENEWAL:
	# 	def __PressYKey(self):
	# 		self.interface.ToggleIkashopBusinessBoard()
	#
	# 	def __PressF8Key(self):
	# 		self.interface.ToggleIkashopSearchShopBoard()



	def __PressNumKey(self,num):
		if app.ENABLE_CINEMACHINE:
			if self.isCinemaMode:
				self.TestFreeCamera(num)
				return

		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):

			if num >= 1 and num <= 9:
				if(chrmgr.IsPossibleEmoticon(-1)):
					chrmgr.SetEmoticon(-1,int(num)-1)
					net.SendEmoticon(int(num)-1)
		else:
			if num >= 1 and num <= 5:
				self.pressNumber(num-1)

	def __ClickBKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			return
		else:
			if constInfo.PVPMODE_ACCELKEY_ENABLE:
				self.ChangePKMode()


	def	__PressJKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			if player.IsMountingHorse():
				net.SendChatPacket("/unmount")
			# else:
				#net.SendChatPacket("/user_horse_ride")
				# if not offlineShopBuilder.IsBuildingPrivateShop():
				# 	for i in xrange(player.INVENTORY_PAGE_SIZE*player.INVENTORY_PAGE_COUNT):
				# 		if player.GetItemIndex(i) in (71114, 71116, 71118, 71120):
				# 			net.SendItemUsePacket(i)
				# 			break
	def	__PressHKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			net.SendChatPacket("/user_horse_ride")
		else:
			self.interface.OpenHelpWindow()

	def	__PressBKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			net.SendChatPacket("/user_horse_back")
		else:
			state = "EMOTICON"
			self.interface.ToggleCharacterWindow(state)

	def	__PressFKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			net.SendChatPacket("/user_horse_feed")
		else:
			app.ZoomCamera(app.CAMERA_TO_POSITIVE)

	def __KeepSidekickWindow(self):
		# The keeper closes the companion's window with the game window.
		import uisidekick
		for keeper in self.updateable:
			if isinstance(keeper, uisidekick.Keeper):
				return
		self.RegisterUpdatable(uisidekick.GetKeeper())

	def __ToggleSidekick(self):
		self.__KeepSidekickWindow()
		import uisidekick
		uisidekick.ToggleWindow()

	def __SidekickWindow(self, *rest):
		self.__KeepSidekickWindow()
		import uisidekick
		uisidekick.OpenWindow()

	def __SidekickInfo(self, *args):
		self.__KeepSidekickWindow()
		import uisidekick
		uisidekick.OnServerInfo(*args)

	def __SidekickNames(self, name="-", place="-", doing="-", *rest):
		import uisidekick
		uisidekick.OnServerNames(name, place, doing)

	def __SidekickGear(self, slot="0", name="-", *rest):
		import uisidekick
		uisidekick.OnServerGear(slot, name)

	def __AutoHuntOff(self, *rest):
		import uiautohunt
		uiautohunt.OnServerOff()

	def __PressGKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			net.SendChatPacket("/ride")
		else:
			if self.ShowNameFlag:
				self.interface.ToggleGuildWindow()
			else:
				app.PitchCamera(app.CAMERA_TO_POSITIVE)

	def __KeepSidekickInventory(self):
		# The keeper closes the companion's bag and skill windows with the
		# game window; the companion's own sends their queued orders.
		self.__KeepSidekickWindow()
		import uisidekickinventory
		for keeper in self.updateable:
			if isinstance(keeper, uisidekickinventory.Keeper):
				return
		self.RegisterUpdatable(uisidekickinventory.GetKeeper())

	def __SidekickEqNone(self, *args):
		self.__KeepSidekickInventory()
		import uisidekickinventory
		uisidekickinventory.OnEqNone(*args)

	def __SidekickEqBegin(self, *args):
		self.__KeepSidekickInventory()
		import uisidekickinventory
		uisidekickinventory.OnEqBegin(*args)

	def __SidekickEqItem(self, *args):
		import uisidekickinventory
		uisidekickinventory.OnEqItem(*args)

	def __SidekickEqEmpty(self, *args):
		import uisidekickinventory
		uisidekickinventory.OnEqEmpty(*args)

	def __SidekickEqEnd(self, *args):
		import uisidekickinventory
		uisidekickinventory.OnEqEnd(*args)

	def __SidekickEqResult(self, *args):
		self.__KeepSidekickInventory()
		import uisidekickinventory
		uisidekickinventory.OnEqResult(*args)

	def __SidekickSkillBegin(self, *args):
		self.__KeepSidekickInventory()
		import uisidekickinventory
		uisidekickinventory.OnSkillBegin(*args)

	def __SidekickSkill(self, *args):
		import uisidekickinventory
		uisidekickinventory.OnSkill(*args)

	def __SidekickSkillEnd(self, *args):
		import uisidekickinventory
		uisidekickinventory.OnSkillEnd(*args)

	def	__ReleaseGKey(self):
		app.PitchCamera(app.CAMERA_STOP)

	def __PressQKey(self):
		if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
			if 0==interfaceModule.IsQBHide:
				interfaceModule.IsQBHide = 1
				self.interface.HideAllQuestButton()
			else:
				interfaceModule.IsQBHide = 0
				self.interface.ShowAllQuestButton()
		else:
			app.RotateCamera(app.CAMERA_TO_NEGATIVE)

	def __PressTABKey(self):
		if app.IsPressed(app.DIK_LCONTROL):
			self.__HideUserInterface()
			return

		self.__PressQuickSlot(5)
		return

	def __ToggleAutoHunt(self):
		import uiautohunt
		uiautohunt.ToggleWindow()

	def __AutoHuntTarget(self, vid="0", *rest):
		import uiautohunt
		uiautohunt.OnServerTarget(vid)

	def __AutoHuntLoot(self, vid="0", x="0", y="0", *rest):
		import uiautohunt
		uiautohunt.OnServerLoot(vid, x, y)

	def __ToggleSprint(self):
		slotIndex = 105 # sprint slot index
		skillIndex = player.GetSkillIndex(slotIndex)
		if skillIndex == 132 and skill.CanUseSkill(skillIndex):
			player.ClickSkillSlot(slotIndex)

	def __SetQuickSlotMode(self):
		self.pressNumber=ui.__mem_func__(self.__PressQuickSlot)

	def __SetQuickPageMode(self):
		self.pressNumber=ui.__mem_func__(self.__SelectQuickPage)

	def __PressQuickSlot(self, localSlotIndex):
		player.RequestUseLocalQuickSlot(localSlotIndex)

	def __SelectQuickPage(self, pageIndex):
		self.quickSlotPageIndex = pageIndex
		player.SetQuickPage(pageIndex)
		self.__SetQuickSlotMode()

	def __HideUserInterface(self):
		# self.ToggleDebugInfo()
		if not self.is_interface_hidden:
			self.__AddFloatingText(localeInfo.HIDE_INTERFACE_TIP, False, 2.5)
			
		self.ToggleAffectBar()
		self.interface.ToggleTaskbarVisibility()
		self.interface.ToggleMinimapVisibility()
		self.interface.ToggleGameButtonVisibility()

		self.is_interface_hidden = not self.is_interface_hidden

	def ToggleAffectBar(self):
		if self.affectBar.IsShow():
			self.affectBar.Hide()
			self.affectBar.vipShower.Hide()
		else:
			self.affectBar.Show()
			self.affectBar.vipShower.Show()

	def ToggleDebugInfo(self):
		self.isShowDebugInfo = not self.isShowDebugInfo

		if self.isShowDebugInfo:
			self.PrintCoord.Show()
			self.FrameRate.Show()
			self.Pitch.Show()
			self.Splat.Show()
			self.TextureNum.Show()
			self.ObjectNum.Show()
			self.ViewDistance.Show()
			self.PrintMousePos.Show()
		else:
			self.PrintCoord.Hide()
			self.FrameRate.Hide()
			self.Pitch.Hide()
			self.Splat.Hide()
			self.TextureNum.Hide()
			self.ObjectNum.Hide()
			self.ViewDistance.Hide()
			self.PrintMousePos.Hide()

	def __BuildDebugInfo(self):
		## Character Position Coordinate
		self.PrintCoord = ui.TextLine()
		self.PrintCoord.SetPosition(wndMgr.GetScreenWidth() - 270, 0)

		## Frame Rate
		self.FrameRate = ui.TextLine()
		self.FrameRate.SetPosition(wndMgr.GetScreenWidth() - 270, 20)

		## Camera Pitch
		self.Pitch = ui.TextLine()
		self.Pitch.SetPosition(wndMgr.GetScreenWidth() - 270, 40)

		## Splat
		self.Splat = ui.TextLine()
		self.Splat.SetPosition(wndMgr.GetScreenWidth() - 270, 60)

		##
		self.PrintMousePos = ui.TextLine()
		self.PrintMousePos.SetPosition(wndMgr.GetScreenWidth() - 270, 80)

		# TextureNum
		self.TextureNum = ui.TextLine()
		self.TextureNum.SetPosition(wndMgr.GetScreenWidth() - 270, 100)

		self.ObjectNum = ui.TextLine()
		self.ObjectNum.SetPosition(wndMgr.GetScreenWidth() - 270, 120)

		self.ViewDistance = ui.TextLine()
		self.ViewDistance.SetPosition(0, 0)

	def __NotifyError(self, msg):
		chat.AppendChat(chat.CHAT_TYPE_INFO, msg)

	def ChangePKMode(self):

		if not app.IsPressed(app.DIK_LCONTROL):
			return

		if player.GetStatus(player.LEVEL)<constInfo.PVPMODE_PROTECTED_LEVEL:
			self.__NotifyError(localeInfo.OPTION_PVPMODE_PROTECT % (constInfo.PVPMODE_PROTECTED_LEVEL))
			return

		curTime = app.GetTime()
		if curTime - self.lastPKModeSendedTime < constInfo.PVPMODE_ACCELKEY_DELAY:
			return

		self.lastPKModeSendedTime = curTime

		curPKMode = player.GetPKMode()
		nextPKMode = curPKMode + 1
		if nextPKMode == player.PK_MODE_PROTECT:
			if 0 == player.GetGuildID():
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.OPTION_PVPMODE_CANNOT_SET_GUILD_MODE)
				nextPKMode = 0
			else:
				nextPKMode = player.PK_MODE_GUILD

		elif nextPKMode == player.PK_MODE_MAX_NUM:
			nextPKMode = 0

		net.SendChatPacket("/PKMode " + str(nextPKMode))
		print "/PKMode " + str(nextPKMode)

	def OnChangePKMode(self):

		self.interface.OnChangePKMode()

		try:
			self.__NotifyError(localeInfo.OPTION_PVPMODE_MESSAGE_DICT[player.GetPKMode()])
		except KeyError:
			print "UNKNOWN PVPMode[%d]" % (player.GetPKMode())

		if constInfo.PVPMODE_TEST_ENABLE:
			curPKMode = player.GetPKMode()
			alignment, grade = chr.testGetPKData()
			self.pkModeNameDict = { 0 : "PEACE", 1 : "REVENGE", 2 : "FREE", 3 : "PROTECT", }
			self.testPKMode.SetText("Current PK Mode : " + self.pkModeNameDict.get(curPKMode, "UNKNOWN"))
			self.testAlignment.SetText("Current Alignment : " + str(alignment) + " (" + localeInfo.TITLE_NAME_LIST[grade] + ")")

	###############################################################################################
	###############################################################################################
	## Game Callback Functions

	# Start
	def StartGame(self):
		self.RefreshInventory()
		self.RefreshEquipment()
		self.RefreshCharacter()
		self.RefreshSkill()

	# Refresh
	def CheckGameButton(self):
		if self.interface:
			self.interface.CheckGameButton()

	def RefreshAlignment(self):
		self.interface.RefreshAlignment()

	def RefreshStatus(self):
		self.CheckGameButton()

		if self.interface:
			self.interface.RefreshStatus()

		if self.playerGauge:
			self.playerGauge.RefreshGauge()

	def RefreshStamina(self):
		self.interface.RefreshStamina()

	def RefreshSkill(self):
		self.CheckGameButton()
		if self.interface:
			self.interface.RefreshSkill()

	def RefreshQuest(self):
		self.interface.RefreshQuest()

	def RefreshMessenger(self):
		self.interface.RefreshMessenger()

	def RefreshGuildInfoPage(self):
		self.interface.RefreshGuildInfoPage()

	def RefreshGuildBoardPage(self):
		self.interface.RefreshGuildBoardPage()

	def RefreshGuildMemberPage(self):
		self.interface.RefreshGuildMemberPage()

	def RefreshGuildMemberPageGradeComboBox(self):
		self.interface.RefreshGuildMemberPageGradeComboBox()

	def RefreshGuildSkillPage(self):
		self.interface.RefreshGuildSkillPage()

	def RefreshGuildGradePage(self):
		self.interface.RefreshGuildGradePage()

	def OnBlockMode(self, mode):
		self.interface.OnBlockMode(mode)

	def OpenQuestWindow(self, skin, idx):
		self.interface.OpenQuestWindow(skin, idx)

	def AskGuildName(self):

		guildNameBoard = uiCommon.InputDialog()
		guildNameBoard.SetTitle(localeInfo.GUILD_NAME)
		guildNameBoard.SetMaxLength(14)
		guildNameBoard.SetAcceptEvent(ui.__mem_func__(self.ConfirmGuildName))
		guildNameBoard.SetCancelEvent(ui.__mem_func__(self.CancelGuildName))
		guildNameBoard.Open()

		self.guildNameBoard = guildNameBoard

	def ConfirmGuildName(self):
		guildName = self.guildNameBoard.GetText()
		if not guildName:
			return

		if net.IsInsultIn(guildName):
			self.PopupMessage(localeInfo.GUILD_CREATE_ERROR_INSULT_NAME)
			return

		if len(guildName) < 2 or len(guildName) > 14:
			return

		net.SendAnswerMakeGuildPacket(guildName)
		self.guildNameBoard.Close()
		self.guildNameBoard = None
		return True

	def CancelGuildName(self):
		self.guildNameBoard.Close()
		self.guildNameBoard = None
		return True

	## Refine
	def PopupMessage(self, msg):
		self.stream.popupWindow.Close()
		self.stream.popupWindow.Open(msg, 0, localeInfo.UI_OK)

	def OpenRefineDialog(self, targetItemPos, nextGradeItemVnum, cost, prob, type=0):
		self.interface.OpenRefineDialog(targetItemPos, nextGradeItemVnum, cost, prob, type)

	def AppendMaterialToRefineDialog(self, vnum, count):
		self.interface.AppendMaterialToRefineDialog(vnum, count)

	def RunUseSkillEvent(self, slotIndex, coolTime):
		self.interface.OnUseSkill(slotIndex, coolTime)

	def ClearAffects(self):
		pass

	def SetAffect(self, affect):
		pass

	def ResetAffect(self, affect):
		pass

	def BINARY_OpenCaptcha(self, expire_time, attempt_count, isFresh):
		self.interface.captchaDialog.SetImage()

		if isFresh:
			self.interface.captchaDialog.Open(expire_time, attempt_count)
			app.SetWindowDisplay(True)

	def BINARY_CloseCaptcha(self):
		self.interface.captchaDialog.Close()
		app.SetWindowDisplay(False)

	def BINARY_CaptchaWrongAttempt(self, left):
		self.interface.captchaDialog.SetAttemptCount(left)

	def BINARY_OpenCaptcha_GameMaster(self, expire_time, attempt_count, pid, playerName, code, isFresh):
		if not constInfo.IS_GAMEMASTER:
			return

		if isFresh:
			dlg = self.interface.CreateGameMasterCaptchaDialog(pid)
			dlg.Open(expire_time, attempt_count, pid, playerName, code)
			dlg.AddLog("Rozpoczeto z kodem |cffffa826%s|r" % code)
			chat.AppendChat(chat.CHAT_TYPE_INFO, "Sprawdzam gracza %s (pid %s) kod: %s" % (playerName, pid , code))
		else:
			dlg = self.interface.GetGameMasterCaptchaDialog(pid)
			if dlg:
				dlg.SetImage(pid)
				dlg.SetCode(code)
				dlg.AddLog(captcha.REQUEST_CHANGE, code)

	def BINARY_CaptchaFeedback_GameMaster(self, pid, type, arg1, arg2):
		if not constInfo.IS_GAMEMASTER:
			return

		dlg = self.interface.GetGameMasterCaptchaDialog(pid)
		if dlg:
			dlg.AddLog(type, arg1, arg2)
		else:
			print pid, type, arg1, arg2

	def BINARY_OpenGameMasterTarget(self, pid, player_name, aid, account_name, is_premium):
		if not self.interface.gameMasterTargetDialog:
			self.interface.gameMasterTargetDialog = uiGameMaster.GameMasterTargetPlayerDialog()

		print pid, player_name, aid, account_name, is_premium
		self.interface.gameMasterTargetDialog.Open(pid, player_name, aid, account_name, is_premium)


	def BINARY_ItemShop_Balance(self, dragonCoins, dragonMarks):
		self.interface.wndItemShop.SetBalance(dragonCoins, dragonMarks)

	def BINARY_ItemShop_Reset_Data(self, isRefreshItemShop):
		print "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ BINARY_ItemShop_Reset_Data"
		if isRefreshItemShop:
			uiItemShop.ResetItemShopData()
		else:
			pass # only auctions

	def BINARY_ItemShop_Refresh_Item(self, index, vnum, count, price, currency, minLevel, promotionPrice,
									 promotionStartTime, promotionEndTime, timeAuctionStartTime, timeAuctionEndTime,
									 timeAuctionAccountLimit, timeAuctionProgress):
		# print index, vnum, count, price, currency, minLevel, promotionPrice, promotionStartTime, promotionEndTime, timeAuctionStartTime, timeAuctionEndTime, timeAuctionAccountLimit, timeAuctionProgress
		item.SelectItem(vnum)

		itemName = item.GetItemName()
		description = ""
		if uiItemShop.ITEMSHOP_CUSTOM_ITEM_DATA.has_key(index):
			data = uiItemShop.ITEMSHOP_CUSTOM_ITEM_DATA[index]
			if data.has_key("itemName"):
				itemName = data["itemName"].replace("$ITEM_NAME", itemName)
			if data.has_key("description"):
				description = data["description"]

		constInfo.ITEMSHOP_DATA[index] = {
			"itemVnum": vnum,
			"itemName": itemName,
			"itemCount": count,
			"itemPrice": price,
			"minLevel": minLevel,
			"currency": currency,
			"description": description,
		}

		current_time = app.GetGlobalTimeStamp()
		if current_time >= promotionStartTime and current_time < promotionEndTime:
			constInfo.ITEMSHOP_DATA[index]["promotion"] = {
				"itemPrice": promotionPrice,
				"endTime": promotionEndTime
			}

		if timeAuctionStartTime > 0:
			constInfo.ITEMSHOP_DATA[index]["timed"] = {
				"startTime": timeAuctionStartTime,
				"endTime": timeAuctionEndTime,
				"amountLeft": timeAuctionProgress,
				"accountLimit": timeAuctionAccountLimit,
			}

	def BINARY_ItemShop_Append_Item(self, index):
		print "BINARY_ItemShop_Append_Item"
		self.interface.wndItemShop.shopContent.CreateItem(index, constInfo.ITEMSHOP_DATA[index])

	def BINARY_ItemShop_Auction_Update(self, index, progress, startTime, endTime, accountLimit):
		print "BINARY_ItemShop_Auction_Update"
		if not constInfo.ITEMSHOP_DATA.has_key(index):
			return

		data = constInfo.ITEMSHOP_DATA[index]
		if data.has_key("timed"):
			data["timed"]["amountLeft"] = progress
			data["timed"]["startTime"] = startTime
			data["timed"]["endTime"] = endTime
			data["timed"]["accountLimit"] = accountLimit

		self.interface.wndItemShop.shopContent.UpdateItem(index, constInfo.ITEMSHOP_DATA[index])

	def BINARY_ItemShop_UseCodeVoucher(self, response):
		eventManager.EventManager().send_event(eventManager.EVENT_USE_CODE_VOUCHER, response)

	def BINARY_ItemShop_Prepare(self, isRefreshItemShop):
		if isRefreshItemShop:
			self.interface.wndItemShop.PrepareShop()
		else:
			self.interface.wndItemShop.SetTimedAuctions()

	def BINARY_ItemShop_HappyHour(self, value, end_time):
		self.interface.wndItemShop.happyHourValue = value
		self.interface.wndItemShop.happyHourEndTime = end_time

	def BINARY_ItemShop_TimeAuction(self, end_time):
		self.interface.wndItemShop.timeAuctionEndTime = end_time

	def BINARY_BusyAction(self, actionType, duration, success):
		print "BINARY_BusyAction", actionType, duration, success
		self.interface.busyActionDialog.Open(actionType, duration, success)

	def BINARY_SpecialShop_Open(self):
		self.interface.wndSpecialShop.Open()

	def BINARY_SpecialShop_Clear(self, fraction):
		self.interface.wndSpecialShop.Clear()
		self.interface.wndSpecialShop.SetFraction(fraction)

	def BINARY_SpecialShop_Buy(self, vnum):
		self.interface.wndSpecialShop.OnBuy(vnum)

	def BINARY_SpecialShop_AddItem(self, vnum, item_vnum, count, rarePct, priceType, price, reqItemsString, limitsString):
		reqItems = []
		reqItemsSplit = reqItemsString.split(',')
		for i in range(0, len(reqItemsSplit), 2):
			reqItemVnum = int(reqItemsSplit[i])
			if reqItemVnum == 0:
				break

			reqItemCount = int(reqItemsSplit[i + 1])
			reqItems.append((reqItemVnum, reqItemCount,))

		limits = []
		limitsSplit = limitsString.split(',')
		for i in range(0, len(limitsSplit), 2):
			limitType = int(limitsSplit[i])
			if limitType == shop.SPECIAL_SHOP_LIMIT_NONE:
				break

			limitValue = int(limitsSplit[i + 1])
			limits.append((limitType, limitValue,))

		itemDict = {
			"itemVnum": item_vnum,
			"count": count,
			"rarePct": rarePct,
			"priceType": priceType,
			"price": price,
			"reqItems": reqItems,
			"limits": limits,
		}

		self.interface.wndSpecialShop.AddItem(vnum, itemDict)

	# UNKNOWN_UPDATE
	def BINARY_NEW_AddAffect(self, type, pointIdx, value, duration, itemVnum=0, flag=0):
		if not constInfo.AFFECT_DICT.has_key(type):
			constInfo.AFFECT_DICT[type] = {}

		constInfo.AFFECT_DICT[type][pointIdx] = (value, duration)
		print "BINARY_NEW_AddAffect %d %d %d %d" % (
		type, chr.NEW_AFFECT_SHOP_PREMIUM, constInfo.AFFECT_DICT.has_key(chr.NEW_AFFECT_SHOP_PREMIUM), flag)
		self.affectBar.AddAffect(type, pointIdx, value, duration, itemVnum, flag)

		eventManager.EventManager().send_event(eventManager.ADD_AFFECT_EVENT, type, pointIdx, value, duration)

	def BINARY_NEW_RemoveAffect(self, type, pointIdx):
		self.affectBar.RemoveAffect(type, pointIdx)
		if constInfo.AFFECT_DICT.has_key(type):
			constInfo.AFFECT_DICT[type].pop(pointIdx)

			if len(constInfo.AFFECT_DICT[type]) == 0:
				constInfo.AFFECT_DICT.pop(type)

	def BINARY_NEW_CurrentChannel(self, channelID):
		if self.interface and self.interface.wndMiniMap:
			self.interface.wndMiniMap.UpdateCurrentChannel(channelID)
		if self.interface.wndMoveChannel:
			self.interface.wndMoveChannel.ingameChannel = int(channelID)

	def UpdateSpecialFlag(self, flag, value):
		value = long(value)
		print "UpdateSpecialFlag %s %ld" % (flag, value)
		constInfo.SPECIAL_FLAG[flag] = value
		eventManager.EventManager().send_event(eventManager.SPECIAL_FLAG_UPDATE, flag, value)

	def ActivateSkillSlot(self, slotIndex):
		if self.interface:
			self.interface.OnActivateSkill(slotIndex)

	def DeactivateSkillSlot(self, slotIndex):
		if self.interface:
			self.interface.OnDeactivateSkill(slotIndex)

	def RefreshEquipment(self):
		if self.interface:
			self.interface.RefreshInventory()

	def RefreshInventory(self):
		if self.interface:
			self.interface.RefreshInventory()

	def RefreshCharacter(self):
		if self.interface:
			self.interface.RefreshCharacter()

	def OnGameOver(self):
		self.CloseTargetBoard()
		self.OpenRestartDialog()

	def OpenRestartDialog(self):
		self.interface.OpenRestartDialog()

	def ChangeCurrentSkill(self, skillSlotNumber):
		self.interface.OnChangeCurrentSkill(skillSlotNumber)

	## TargetBoard
	def SetPCTargetBoard(self, vid, name):
		self.targetBoard.Open(vid, name)

		if app.IsPressed(app.DIK_LCONTROL):

			if not player.IsSameEmpire(vid):
				return

			if player.IsMainCharacterIndex(vid):
				return
			elif chr.INSTANCE_TYPE_BUILDING == chr.GetInstanceType(vid):
				return

			self.interface.OpenWhisperDialog(name)


	def RefreshTargetBoardByVID(self, vid):
		self.targetBoard.RefreshByVID(vid)

	def RefreshTargetBoardByName(self, name):
		self.targetBoard.RefreshByName(name)

	def __RefreshTargetBoard(self):
		self.targetBoard.Refresh()

	def SetHPTargetBoard(self, vid, hpPercentage):
		if vid != self.targetBoard.GetTargetVID():
			self.targetBoard.ResetTargetBoard()
			self.targetBoard.SetEnemyVID(vid)

		self.targetBoard.SetHP(hpPercentage)
		self.targetBoard.Show()

	def StartTargetAffect(self, vid):
		if vid != self.targetBoard.GetTargetVID():
			return

		self.targetBoard.StartTargetAffect()

	def SetTargetAffect(self, vid, affect, duration):
		if vid != self.targetBoard.GetTargetVID():
			return

		if not self.targetBoard.IsShow():
			return

		self.targetBoard.AddTargetAffect(affect, duration)

	def ClearTargetAffect(self, vid):
		if vid != self.targetBoard.GetTargetVID():
			return

		self.targetBoard.ClearTargetAffect()

	def CloseTargetBoardIfDifferent(self, vid):
		if vid != self.targetBoard.GetTargetVID():
			self.targetBoard.Close()

	def CloseTargetBoard(self):
		self.targetBoard.Close()

	## View Equipment
	def OpenEquipmentDialog(self, vid):
		self.interface.OpenEquipmentDialog(vid)

	def SetEquipmentDialogItem(self, vid, slotIndex, vnum, count):
		self.interface.SetEquipmentDialogItem(vid, slotIndex, vnum, count)

	def SetEquipmentDialogSocket(self, vid, slotIndex, socketIndex, value):
		self.interface.SetEquipmentDialogSocket(vid, slotIndex, socketIndex, value)

	def SetEquipmentDialogAttr(self, vid, slotIndex, attrIndex, type, value):
		self.interface.SetEquipmentDialogAttr(vid, slotIndex, attrIndex, type, value)

	# SHOW_LOCAL_MAP_NAME
	def ShowMapName(self, mapName, x, y):

		if self.mapNameShower:
			self.mapNameShower.ShowMapName(mapName, x, y)

		if self.interface:
			self.interface.SetMapName(mapName)
	# END_OF_SHOW_LOCAL_MAP_NAME

	def BINARY_OpenAtlasWindow(self):
		self.interface.BINARY_OpenAtlasWindow()

	## Chat
	def OnRecvWhisper(self, mode, name, line):
		if mode == chat.WHISPER_TYPE_GM:
			self.interface.RegisterGameMasterName(name)
		elif mode == chat.WHISPER_TYPE_MYSHOP:
			current_timestamp = time.time()
			time_struct = time.localtime(current_timestamp)
			formatted_time = time.strftime("%H:%M:%S", time_struct)

			line = "|cff76b9de[%s]|r %s" % (formatted_time, line)

		chat.AppendWhisper(mode, name, line)
		self.interface.RecvWhisper(name, mode)
		self.interface.PushWhisperMessageStack(name, mode, line)

	def OnRecvWhisperSystemMessage(self, mode, name, line):
		self.OnRecvWhisper(chat.WHISPER_TYPE_SYSTEM, name, line)

	def OnRecvWhisperError(self, mode, name, line):
		msg = line
		if localeInfo.WHISPER_ERROR.has_key(mode):
			msg = localeInfo.WHISPER_ERROR[mode](name)
		else:
			msg = "Whisper Unknown Error(mode=%d, name=%s)" % (mode, name)
		self.OnRecvWhisper(chat.WHISPER_TYPE_SYSTEM, name, msg)

	def RecvWhisper(self, name):
		self.interface.RecvWhisper(name)

	def OnGoldUpdate(self, money):
		print money
		moneyLong = long(money)
		if moneyLong > 0:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.GAME_PICK_MONEY % localeInfo.NumberToMoneyString(moneyLong))
		self.OnGoldSet(-1)

	def OnGoldSet(self, totalMoney):
		self.interface.wndInventory.RefreshGold(long(totalMoney))

	def OnPickMoney(self, money):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.GAME_PICK_MONEY % localeInfo.MoneyFormat(money)) # @fixme020

	if app.ENABLE_CHEQUE_SYSTEM:
		def OnPickCheque(self, cheque):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.GAME_PICK_CHEQUE % (cheque))

	def OnShopError(self, type):
		try:
			self.PopupMessage(localeInfo.SHOP_ERROR_DICT[type])
		except KeyError:
			self.PopupMessage(localeInfo.SHOP_ERROR_UNKNOWN % (type))

	def OnSafeBoxError(self):
		self.PopupMessage(localeInfo.SAFEBOX_ERROR)

	def OnFishingSuccess(self, isFish, fishName):
		chat.AppendChatWithDelay(chat.CHAT_TYPE_INFO, localeInfo.FISHING_SUCCESS(isFish, fishName), 2000)

	# ADD_FISHING_MESSAGE
	def OnFishingNotifyUnknown(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.FISHING_UNKNOWN)

	def OnFishingWrongPlace(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.FISHING_WRONG_PLACE)
	# END_OF_ADD_FISHING_MESSAGE

	def OnFishingNotify(self, isFish, fishName):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.FISHING_NOTIFY(isFish, fishName))

	def OnFishingFailure(self):
		chat.AppendChatWithDelay(chat.CHAT_TYPE_INFO, localeInfo.FISHING_FAILURE, 2000)

	def OnFishingGameUpdate(self, top_bar_pos, player_pos):
		eventManager.EventManager().send_event(uiFishing.EVENT_FISHING_GAME_UPDATE, top_bar_pos, player_pos)

	def OnFishingPlayerUpdate(self, player_pos):
		eventManager.EventManager().send_event(uiFishing.EVENT_FISHING_PLAYER_UPDATE, player_pos)

	def OnCannotPickItem(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.GAME_CANNOT_PICK_ITEM)

	# MINING
	def OnCannotMining(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.GAME_CANNOT_MINING)
	# END_OF_MINING

	def OnCannotUseSkill(self, vid, type):
		if localeInfo.USE_SKILL_ERROR_TAIL_DICT.has_key(type):
			textTail.RegisterInfoTail(vid, localeInfo.USE_SKILL_ERROR_TAIL_DICT[type])

		if localeInfo.USE_SKILL_ERROR_CHAT_DICT.has_key(type):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.USE_SKILL_ERROR_CHAT_DICT[type])

	def	OnCannotShotError(self, vid, type):
		textTail.RegisterInfoTail(vid, localeInfo.SHOT_ERROR_TAIL_DICT.get(type, localeInfo.SHOT_ERROR_UNKNOWN % (type)))

	## PointReset
	def StartPointReset(self):
		self.interface.OpenPointResetDialog()

	## Shop
	def StartShop(self, vid, unlockCount, isPremium):
		if chr.IsNPC(vid):
			self.interface.OpenShopDialog(vid)
		else:
			self.interface.offlineShopGuest.OpenNormal(vid, unlockCount, isPremium)

	def EndShop(self):
		self.interface.CloseShopDialog()

	def RefreshShop(self):
		self.interface.RefreshShopDialog()

	def SetShopSellingPrice(self, Price):
		pass

	## Exchange
	def StartExchange(self):
		self.interface.StartExchange()

	def EndExchange(self):
		self.interface.EndExchange()

	def RefreshExchange(self):
		self.interface.RefreshExchange()

	## Party
	def RecvPartyInviteQuestion(self, leaderVID, leaderName):
		partyInviteQuestionDialog = uiCommon.QuestionDialog()
		partyInviteQuestionDialog.SetText(leaderName + localeInfo.PARTY_DO_YOU_JOIN)
		partyInviteQuestionDialog.SetAcceptEvent(lambda arg=True: self.AnswerPartyInvite(arg))
		partyInviteQuestionDialog.SetCancelEvent(lambda arg=False: self.AnswerPartyInvite(arg))
		partyInviteQuestionDialog.Open()
		partyInviteQuestionDialog.partyLeaderVID = leaderVID
		self.partyInviteQuestionDialog = partyInviteQuestionDialog

	def AnswerPartyInvite(self, answer):

		if not self.partyInviteQuestionDialog:
			return

		partyLeaderVID = self.partyInviteQuestionDialog.partyLeaderVID

		distance = player.GetCharacterDistance(partyLeaderVID)
		if distance < 0.0 or distance > 5000:
			answer = False

		net.SendPartyInviteAnswerPacket(partyLeaderVID, answer)

		self.partyInviteQuestionDialog.Close()
		self.partyInviteQuestionDialog = None

	def AddPartyMember(self, pid, name):
		self.interface.AddPartyMember(pid, name)

	def UpdatePartyMemberInfo(self, pid):
		self.interface.UpdatePartyMemberInfo(pid)

	def RemovePartyMember(self, pid):
		self.interface.RemovePartyMember(pid)
		self.__RefreshTargetBoard()

	def LinkPartyMember(self, pid, vid):
		self.interface.LinkPartyMember(pid, vid)

	def UnlinkPartyMember(self, pid):
		self.interface.UnlinkPartyMember(pid)

	def UnlinkAllPartyMember(self):
		self.interface.UnlinkAllPartyMember()

	def ExitParty(self):
		self.interface.ExitParty()
		self.RefreshTargetBoardByVID(self.targetBoard.GetTargetVID())

	def ChangePartyParameter(self, distributionMode):
		self.interface.ChangePartyParameter(distributionMode)

	## Messenger
	def OnMessengerAddFriendQuestion(self, name):
		messengerAddFriendQuestion = uiCommon.QuestionDialog2()
		messengerAddFriendQuestion.SetText1(localeInfo.MESSENGER_DO_YOU_ACCEPT_ADD_FRIEND_1 % (name))
		messengerAddFriendQuestion.SetText2(localeInfo.MESSENGER_DO_YOU_ACCEPT_ADD_FRIEND_2)
		messengerAddFriendQuestion.SetAcceptEvent(ui.__mem_func__(self.OnAcceptAddFriend))
		messengerAddFriendQuestion.SetCancelEvent(ui.__mem_func__(self.OnDenyAddFriend))
		messengerAddFriendQuestion.Open()
		messengerAddFriendQuestion.name = name
		self.messengerAddFriendQuestion = messengerAddFriendQuestion

	def OnAcceptAddFriend(self):
		name = self.messengerAddFriendQuestion.name
		net.SendChatPacket("/messenger_auth y " + name)
		self.OnCloseAddFriendQuestionDialog()
		return True

	def OnDenyAddFriend(self):
		name = self.messengerAddFriendQuestion.name
		net.SendChatPacket("/messenger_auth n " + name)
		self.OnCloseAddFriendQuestionDialog()
		return True

	def OnCloseAddFriendQuestionDialog(self):
		self.messengerAddFriendQuestion.Close()
		self.messengerAddFriendQuestion = None
		return True

	## SafeBox
	def OpenSafeboxWindow(self, size):
		self.interface.OpenSafeboxWindow(size)

	def RefreshSafebox(self):
		self.interface.RefreshSafebox()

	def RefreshSafeboxMoney(self):
		self.interface.RefreshSafeboxMoney()

	# ITEM_MALL
	def OpenMallWindow(self, size):
		self.interface.OpenMallWindow(size)

	def RefreshMall(self):
		self.interface.RefreshMall()
	# END_OF_ITEM_MALL

	## Guild
	def RecvGuildInviteQuestion(self, guildID, guildName):
		guildInviteQuestionDialog = uiCommon.QuestionDialog()
		guildInviteQuestionDialog.SetText(guildName + localeInfo.GUILD_DO_YOU_JOIN)
		guildInviteQuestionDialog.SetAcceptEvent(lambda arg=True: self.AnswerGuildInvite(arg))
		guildInviteQuestionDialog.SetCancelEvent(lambda arg=False: self.AnswerGuildInvite(arg))
		guildInviteQuestionDialog.Open()
		guildInviteQuestionDialog.guildID = guildID
		self.guildInviteQuestionDialog = guildInviteQuestionDialog

	def AnswerGuildInvite(self, answer):

		if not self.guildInviteQuestionDialog:
			return

		guildLeaderVID = self.guildInviteQuestionDialog.guildID
		net.SendGuildInviteAnswerPacket(guildLeaderVID, answer)

		self.guildInviteQuestionDialog.Close()
		self.guildInviteQuestionDialog = None


	def DeleteGuild(self):
		self.interface.DeleteGuild()

	## Clock
	def ShowClock(self, second):
		self.interface.ShowClock(second)

	def HideClock(self):
		self.interface.HideClock()

	## Emotion
	def BINARY_ActEmotion(self, emotionIndex):
		if self.interface.wndCharacter:
			self.interface.wndCharacter.ActEmotion(emotionIndex)

	###############################################################################################
	###############################################################################################
	## Keyboard Functions

	def CheckFocus(self):
		if False == self.IsFocus():
			if True == self.interface.IsOpenChat():
				self.interface.ToggleChat()

			self.SetFocus()

	def SaveScreen(self):
		print "save screen"

		# SCREENSHOT_CWDSAVE
		if SCREENSHOT_CWDSAVE:
			if not os.path.exists(os.getcwd()+os.sep+"screenshot"):
				os.mkdir(os.getcwd()+os.sep+"screenshot")

			(succeeded, name) = grp.SaveScreenShotToPath(os.getcwd()+os.sep+"screenshot"+os.sep)
		elif SCREENSHOT_DIR:
			(succeeded, name) = grp.SaveScreenShot(SCREENSHOT_DIR)
		else:
			(succeeded, name) = grp.SaveScreenShot()
		# END_OF_SCREENSHOT_CWDSAVE

		if succeeded:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SCREENSHOT_SAVE1 % name)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SCREENSHOT_SAVE_FAILURE)

	def ShowConsole(self):
		if debugInfo.IsDebugMode() or True == self.consoleEnable:
			player.EndKeyWalkingImmediately()
			self.console.OpenWindow()

	def ShowName(self):
		self.ShowNameFlag = True
		self.playerGauge.EnableShowAlways()
		player.SetQuickPage(self.quickSlotPageIndex+1)

	# ADD_ALWAYS_SHOW_NAME
	def __IsShowName(self):

		if systemSetting.IsAlwaysShowName() or systemSetting.IsGroupShowName():
			return True

		if self.ShowNameFlag:
			return True

		return False
	# END_OF_ADD_ALWAYS_SHOW_NAME

	def HideName(self):
		self.ShowNameFlag = False
		self.playerGauge.DisableShowAlways()
		player.SetQuickPage(self.quickSlotPageIndex)

	def ShowMouseImage(self):
		self.interface.ShowMouseImage()

	def HideMouseImage(self):
		self.interface.HideMouseImage()

	def StartAttack(self):
		player.SetAttackKeyState(True)

	def EndAttack(self):
		player.SetAttackKeyState(False)

	def MoveUp(self):
		player.SetSingleDIKKeyState(app.DIK_UP, True)

	def MoveDown(self):
		player.SetSingleDIKKeyState(app.DIK_DOWN, True)

	def MoveLeft(self):
		player.SetSingleDIKKeyState(app.DIK_LEFT, True)

	def MoveRight(self):
		player.SetSingleDIKKeyState(app.DIK_RIGHT, True)

	def StopUp(self):
		player.SetSingleDIKKeyState(app.DIK_UP, False)

	def StopDown(self):
		player.SetSingleDIKKeyState(app.DIK_DOWN, False)

	def StopLeft(self):
		player.SetSingleDIKKeyState(app.DIK_LEFT, False)

	def StopRight(self):
		player.SetSingleDIKKeyState(app.DIK_RIGHT, False)

	def PickUpItem(self):
		player.PickCloseItem()

	def PickUpNearbyItems(self):
		import pickupnearby
		pickupnearby.Request()

	###############################################################################################
	###############################################################################################
	## Event Handler

	def OnKeyDown(self, key):
		if self.interface and self.interface.wndWeb and self.interface.wndWeb.IsShow():
			return

		if key == app.DIK_ESC:
			self.RequestDropItem(False)
			constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

		if self.onPressKeyDict:
			try:
				self.onPressKeyDict[key]()
			except KeyError:
				pass
			except:
				raise

		return True

	def OnKeyUp(self, key):
		if self.onClickKeyDict:
			try:
				self.onClickKeyDict[key]()
			except KeyError:
				pass
			except:
				raise

		return True

	def OnMouseLeftButtonDown(self):
		if self.interface.BUILD_OnMouseLeftButtonDown():
			return

		if mouseModule.mouseController.isAttached():
			self.CheckFocus()
		else:
			hyperlink = ui.GetHyperlink()
			if hyperlink:
				return
			else:
				self.CheckFocus()
				player.SetMouseState(player.MBT_LEFT, player.MBS_PRESS);

		return True

	def OnMouseLeftButtonUp(self):
		if self.interface.BUILD_OnMouseLeftButtonUp():
			return

		if mouseModule.mouseController.isAttached():

			attachedType = mouseModule.mouseController.GetAttachedType()
			attachedItemIndex = mouseModule.mouseController.GetAttachedItemIndex()
			attachedItemSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			attachedItemCount = mouseModule.mouseController.GetAttachedItemCount()
			attachedChequeCount = mouseModule.mouseController.GetAttachedChequeCount() if app.ENABLE_CHEQUE_SYSTEM else 0

			## QuickSlot
			if player.SLOT_TYPE_QUICK_SLOT == attachedType:
				player.RequestDeleteGlobalQuickSlot(attachedItemSlotPos)

			## Inventory
			elif player.SLOT_TYPE_INVENTORY == attachedType:
				if player.ITEM_MONEY == attachedItemIndex:
					self.__PutMoney(attachedType, attachedItemCount, self.PickingCharacterIndex, attachedChequeCount)
				else:
					self.__PutItem(attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount, self.PickingCharacterIndex)

			## DragonSoul
			elif player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedType:
				self.__PutItem(attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount, self.PickingCharacterIndex)

			mouseModule.mouseController.DeattachObject()

		else:
			hyperlink = ui.GetHyperlink()
			if hyperlink:
				if app.IsPressed(app.DIK_LALT):
					link = chat.GetLinkFromHyperlink(hyperlink)
					ime.PasteString(link)
				else:
					self.interface.MakeHyperlinkTooltip(hyperlink)
				return
			else:
				player.SetMouseState(player.MBT_LEFT, player.MBS_CLICK)

		#player.EndMouseWalking()
		return True

	def __PutItem(self, attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount, dstChrID):
		if player.SLOT_TYPE_INVENTORY == attachedType or player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedType:
			attachedInvenType = player.SlotTypeToInvenType(attachedType)
			if True == chr.HasInstance(self.PickingCharacterIndex) and player.GetMainCharacterIndex() != dstChrID:
				if player.IsEquipmentSlot(attachedItemSlotPos) and player.SLOT_TYPE_DRAGON_SOUL_INVENTORY != attachedType:
					self.stream.popupWindow.Close()
					self.stream.popupWindow.Open(localeInfo.EXCHANGE_FAILURE_EQUIP_ITEM, 0, localeInfo.UI_OK)
				else:
					if chr.IsNPC(dstChrID):
						net.SendGiveItemPacket(dstChrID, attachedInvenType, attachedItemSlotPos, attachedItemCount)
					else:
						net.SendExchangeStartPacket(dstChrID)
						net.SendExchangeItemAddPacket(attachedInvenType, attachedItemSlotPos, 0)
			else:
				self.__DropItem(attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount)

	def __PutMoney(self, attachedType, attachedMoney, dstChrID, attachedChequeCount=0):
		if True == chr.HasInstance(dstChrID) and player.GetMainCharacterIndex() != dstChrID:
			net.SendExchangeStartPacket(dstChrID)
			net.SendExchangeElkAddPacket(attachedMoney)
			if app.ENABLE_CHEQUE_SYSTEM and attachedChequeCount:
				net.SendExchangeChequeAddPacket(attachedChequeCount)
		else:
			if app.ENABLE_CHEQUE_SYSTEM and attachedChequeCount > 0 and not attachedMoney:
				self.__DropCheque(attachedType, attachedChequeCount)
				return
			self.__DropMoney(attachedType, attachedMoney)

	def __DropMoney(self, attachedType, attachedMoney):
		if offlineShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.DROP_ITEM_FAILURE_PRIVATE_SHOP)
			return
		# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP

		if attachedMoney>=1000:
			self.stream.popupWindow.Close()
			self.stream.popupWindow.Open(localeInfo.DROP_MONEY_FAILURE_1000_OVER, 0, localeInfo.UI_OK)
			return

		itemDropQuestionDialog = uiCommon.QuestionDialog()
		itemDropQuestionDialog.SetText(localeInfo.DO_YOU_DROP_MONEY % (attachedMoney))
		itemDropQuestionDialog.SetAcceptEvent(lambda arg=True: self.RequestDropItem(arg))
		itemDropQuestionDialog.SetCancelEvent(lambda arg=False: self.RequestDropItem(arg))
		itemDropQuestionDialog.Open()
		itemDropQuestionDialog.dropType = attachedType
		itemDropQuestionDialog.dropCount = attachedMoney
		itemDropQuestionDialog.dropNumber = player.ITEM_MONEY
		self.itemDropQuestionDialog = itemDropQuestionDialog

	if app.ENABLE_CHEQUE_SYSTEM:
		def __DropCheque(self, attachedType, attachedMoney):
			# PRIVATESHOP_DISABLE_ITEM_DROP
			if offlineShopBuilder.IsBuildingPrivateShop():
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.DROP_ITEM_FAILURE_PRIVATE_SHOP)
				return
			# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP

			if attachedMoney>=1000:
				self.stream.popupWindow.Close()
				self.stream.popupWindow.Open(localeInfo.DROP_CHEQUE_FAILURE_1000_OVER, 0, localeInfo.UI_OK)
				return

			itemDropQuestionDialog = uiCommon.QuestionDialog()
			itemDropQuestionDialog.SetText(localeInfo.DO_YOU_DROP_CHEQUE % (attachedMoney))
			itemDropQuestionDialog.SetAcceptEvent(lambda arg=True: self.RequestDropItem(arg))
			itemDropQuestionDialog.SetCancelEvent(lambda arg=False: self.RequestDropItem(arg))
			itemDropQuestionDialog.Open()
			itemDropQuestionDialog.dropType = attachedType
			itemDropQuestionDialog.dropCount = attachedMoney
			itemDropQuestionDialog.dropNumber = player.ITEM_CHEQUE
			self.itemDropQuestionDialog = itemDropQuestionDialog


	def __DropItem(self, attachedType, attachedItemIndex, attachedItemSlotPos, attachedItemCount):
		if offlineShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.DROP_ITEM_FAILURE_PRIVATE_SHOP)
			return
		# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP

		if player.SLOT_TYPE_INVENTORY == attachedType and player.IsEquipmentSlot(attachedItemSlotPos):
			self.stream.popupWindow.Close()
			self.stream.popupWindow.Open(localeInfo.DROP_ITEM_FAILURE_EQUIP_ITEM, 0, localeInfo.UI_OK)

		else:
			if player.SLOT_TYPE_INVENTORY == attachedType:
				dropItemIndex = player.GetItemIndex(attachedItemSlotPos)

				item.SelectItem(dropItemIndex)
				dropItemName = item.GetItemName()

				## Question Text
				questionText = localeInfo.HOW_MANY_ITEM_DO_YOU_DROP(dropItemName, attachedItemCount)

				## Dialog
				itemDropQuestionDialog = uiCommon.QuestionDialog3Options()
				itemDropQuestionDialog.SetText(questionText)
				itemDropQuestionDialog.SetAcceptEvent(lambda arg=True: self.RequestDropItem(arg))
				itemDropQuestionDialog.SetMiddleEvent(lambda arg=attachedItemSlotPos: self.AskDeleteItem(attachedItemSlotPos))
				itemDropQuestionDialog.SetCancelEvent(lambda arg=False: self.RequestDropItem(arg))
				itemDropQuestionDialog.SetAcceptText(localeInfo.ITEM_ACTION_DROP)
				itemDropQuestionDialog.SetMiddleText(localeInfo.ITEM_ACTON_DELETE)
				itemDropQuestionDialog.SetCancelText(localeInfo.UI_CANCEL)
				itemDropQuestionDialog.Open()
				itemDropQuestionDialog.dropType = attachedType
				itemDropQuestionDialog.dropNumber = attachedItemSlotPos
				itemDropQuestionDialog.dropCount = attachedItemCount
				self.itemDropQuestionDialog = itemDropQuestionDialog

				constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)
			elif player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedType:
				dropItemIndex = player.GetItemIndex(player.DRAGON_SOUL_INVENTORY, attachedItemSlotPos)

				item.SelectItem(dropItemIndex)
				dropItemName = item.GetItemName()

				## Question Text
				questionText = localeInfo.HOW_MANY_ITEM_DO_YOU_DROP(dropItemName, attachedItemCount)

				## Dialog
				itemDropQuestionDialog = uiCommon.QuestionDialog3Options()
				itemDropQuestionDialog.SetText(questionText)
				itemDropQuestionDialog.SetAcceptEvent(lambda arg=True: self.RequestDropItem(arg))
				itemDropQuestionDialog.SetMiddleEvent(lambda arg=attachedItemSlotPos: self.AskDeleteItem(attachedItemSlotPos))
				itemDropQuestionDialog.SetCancelEvent(lambda arg=False: self.RequestDropItem(arg))
				itemDropQuestionDialog.SetAcceptText(localeInfo.ITEM_ACTION_DROP)
				itemDropQuestionDialog.SetMiddleText(localeInfo.ITEM_ACTON_DELETE)
				itemDropQuestionDialog.SetCancelText(localeInfo.UI_CANCEL)
				itemDropQuestionDialog.Open()
				itemDropQuestionDialog.dropType = attachedType
				itemDropQuestionDialog.dropNumber = attachedItemSlotPos
				itemDropQuestionDialog.dropCount = attachedItemCount
				self.itemDropQuestionDialog = itemDropQuestionDialog

				constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

	def AskDeleteItem(self, itemSlotPos):
		if not self.itemDropQuestionDialog:
			return

		dropItemIndex = player.GetItemIndex(itemSlotPos)
		item.SelectItem(dropItemIndex)
		dropItemName = item.GetItemName()
		itemCount = player.GetItemCount(itemSlotPos)

		self.itemDropQuestionDialog.Close()
		self.itemDropQuestionDialog = None

		dlg = uiCommon.QuestionDialog2()
		dlg.SetText1(localeInfo.CONFIRM_DELETE_ITEM_QUESTION % (itemCount, dropItemName))
		dlg.SetText2(localeInfo.CONFIRM_DELETE_ITEM_QUESTION2)
		dlg.SetAcceptEvent(lambda arg=itemSlotPos: self.RequestDeleteItem(itemSlotPos))
		dlg.SetCancelEvent(lambda arg=False: self.RequestDropItem(arg))
		dlg.Open()

		self.itemDropQuestionDialog = dlg

	def RequestDeleteItem(self, itemSlotPos):
		if not self.itemDropQuestionDialog:
			return

		net.SendChatPacket("/inventory_remove %d" % itemSlotPos)
		self.itemDropQuestionDialog.Close()
		self.itemDropQuestionDialog = None
		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	def RequestDropItem(self, answer):
		if not self.itemDropQuestionDialog:
			return

		if answer:
			dropType = self.itemDropQuestionDialog.dropType
			dropCount = self.itemDropQuestionDialog.dropCount
			dropNumber = self.itemDropQuestionDialog.dropNumber

			if player.SLOT_TYPE_INVENTORY == dropType:
				if app.ENABLE_CHEQUE_SYSTEM:
					if dropNumber == player.ITEM_MONEY:
						net.SendGoldDropPacketNew(dropCount)
						snd.PlaySound("sound/ui/money.wav")
					elif not app.DISABLE_CHEQUE_DROP and dropNumber == player.ITEM_CHEQUE:
						net.SendGoldChequePacketNew(dropCount)
						snd.PlaySound("sound/ui/money.wav")
					else:
						# PRIVATESHOP_DISABLE_ITEM_DROP
						self.__SendDropItemPacket(dropNumber, dropCount)
						# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP
				else:
					if dropNumber == player.ITEM_MONEY:
						return
						# net.SendGoldDropPacketNew(dropCount)
						# snd.PlaySound("sound/ui/money.wav")
					else:
						# PRIVATESHOP_DISABLE_ITEM_DROP
						self.__SendDropItemPacket(dropNumber, dropCount)
						# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP
			elif player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == dropType:
					# PRIVATESHOP_DISABLE_ITEM_DROP
					self.__SendDropItemPacket(dropNumber, dropCount, player.DRAGON_SOUL_INVENTORY)
					# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP

		self.itemDropQuestionDialog.Close()
		self.itemDropQuestionDialog = None

		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	# PRIVATESHOP_DISABLE_ITEM_DROP
	def __SendDropItemPacket(self, itemVNum, itemCount, itemInvenType = player.INVENTORY):
		if offlineShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.DROP_ITEM_FAILURE_PRIVATE_SHOP)
			return

		net.SendItemDropPacketNew(itemInvenType, itemVNum, itemCount)
	# END_OF_PRIVATESHOP_DISABLE_ITEM_DROP

	def OnMouseRightButtonDown(self):

		self.CheckFocus()

		if True == mouseModule.mouseController.isAttached():
			mouseModule.mouseController.DeattachObject()

		else:
			player.SetMouseState(player.MBT_RIGHT, player.MBS_PRESS)

		return True

	def OnMouseRightButtonUp(self):
		if True == mouseModule.mouseController.isAttached():
			return True

		player.SetMouseState(player.MBT_RIGHT, player.MBS_CLICK)
		return True

	def OnMouseMiddleButtonDown(self):
		player.SetMouseMiddleButtonState(player.MBS_PRESS)

	def OnMouseMiddleButtonUp(self):
		player.SetMouseMiddleButtonState(player.MBS_CLICK)

	def OnPerSecondFixedUpdate(self):
		self.interface.offlineShopManage.OnFixedUpdate()
		# self.interface.dlgShop.OnFixedUpdate()
		self.__UpdateNightMode()
		self.interface.wndPlayerStat.UpdateSessionTime()
		self.targetBoard.UpdateTargetAffectDuration()

		wndItemShop = self.interface.wndItemShop
		if wndItemShop and wndItemShop.IsShow() and wndItemShop.shopContent and wndItemShop.shopContent.IsShow():
			wndItemShop.shopContent.OnPerSecondUpdate()
			
		self.interface.captchaDialog.OnUpdatePerSecond()
		for dlg in self.interface.gameMaster_CaptchaDialogs.values():
			if dlg:
				dlg.OnUpdatePerSecond()

	def OnUpdate(self):
		app.UpdateGame()

		# Panel GM: patrz __gmCheckSent w __init__ - kilkaset klatek po wejsciu
		# do swiata pytamy serwer, czy ta postac jest GM (odpowiedz: SetGMFlag).
		if not self.__gmCheckSent:
			self.__gmCheckFrames += 1
			if self.__gmCheckFrames > 300:
				self.__gmCheckSent = True
				net.SendChatPacket("/gmpanel_check_gm")

		constInfo.FIXED_TIME_SINCE_START += FIXED_TIMESTEP_UPDATE
		self.oneSecondTimer += FIXED_TIMESTEP_UPDATE
		if self.oneSecondTimer > 10000:
			self.OnPerSecondFixedUpdate()
			self.oneSecondTimer = 0

		eventManager.EventManager().Update()

		floating_text_count = len(self.floating_texts)
		for i in range(0, floating_text_count):
			index = floating_text_count - 1 - i
			item = self.floating_texts[index]
			if item.OnUpdate():
				self.floating_texts.pop(index)

		self.affectBar.OnFixedUpdate()

		if self.interface.fishingGameDialog.IsShow():
			self.interface.fishingGameDialog.OnFixedUpdate(FIXED_TIMESTEP_UPDATE)

		self.tweenMgr.OnUpdate()

		if self.mapNameShower.IsShow():
			self.mapNameShower.Update()

		if self.isShowDebugInfo:
			self.UpdateDebugInfo()

		if self.enableXMasBoom:
			self.__XMasBoom_Update()

		if self.updateable:
			for updateableObject in self.updateable:
				if updateableObject.CanUpdate():
					updateableObject.OnUpdate()

		self.interface.BUILD_OnUpdate()


	def UpdateDebugInfo(self):
		#
		(x, y, z) = player.GetMainCharacterPosition()
		nUpdateTime = app.GetUpdateTime()
		nUpdateFPS = app.GetUpdateFPS()
		nRenderFPS = app.GetRenderFPS()
		nFaceCount = app.GetFaceCount()
		fFaceSpeed = app.GetFaceSpeed()
		nST=background.GetRenderShadowTime()
		(fAveRT, nCurRT) =  app.GetRenderTime()
		(iNum, fFogStart, fFogEnd, fFarCilp) = background.GetDistanceSetInfo()
		(iPatch, iSplat, fSplatRatio, sTextureNum) = background.GetRenderedSplatNum()
		if iPatch == 0:
			iPatch = 1

		#(dwRenderedThing, dwRenderedCRC) = background.GetRenderedGraphicThingInstanceNum()

		self.PrintCoord.SetText("Coordinate: %.2f %.2f %.2f ATM: %d" % (x, y, z, app.GetAvailableTextureMemory()/(1024*1024)))
		xMouse, yMouse = wndMgr.GetMousePosition()
		self.PrintMousePos.SetText("MousePosition: %d %d" % (xMouse, yMouse))

		self.FrameRate.SetText("UFPS: %3d UT: %3d FS %.2f" % (nUpdateFPS, nUpdateTime, fFaceSpeed))

		if fAveRT>1.0:
			self.Pitch.SetText("RFPS: %3d RT:%.2f(%3d) FC: %d(%.2f) " % (nRenderFPS, fAveRT, nCurRT, nFaceCount, nFaceCount/fAveRT))

		self.Splat.SetText("PATCH: %d SPLAT: %d BAD(%.2f)" % (iPatch, iSplat, fSplatRatio))
		#self.Pitch.SetText("Pitch: %.2f" % (app.GetCameraPitch())
		#self.TextureNum.SetText("TN : %s" % (sTextureNum))
		#self.ObjectNum.SetText("GTI : %d, CRC : %d" % (dwRenderedThing, dwRenderedCRC))
		self.ViewDistance.SetText("Num : %d, FS : %f, FE : %f, FC : %f" % (iNum, fFogStart, fFogEnd, fFarCilp))

	def OnRender(self):
		app.RenderGame()

		if self.console.Console.collision:
			background.RenderCollision()
			chr.RenderCollision()

		(x, y) = app.GetCursorPosition()

		########################
		# Picking
		########################
		textTail.UpdateAllTextTail()

		if True == wndMgr.IsPickedWindow(self.hWnd):

			self.PickingCharacterIndex = chr.Pick()

			if -1 != self.PickingCharacterIndex:
				textTail.ShowCharacterTextTail(self.PickingCharacterIndex)
			if 0 != self.targetBoard.GetTargetVID():
				textTail.ShowCharacterTextTail(self.targetBoard.GetTargetVID())

			# ADD_ALWAYS_SHOW_NAME
			if not self.__IsShowName():
				self.PickingItemIndex = item.Pick()
				if -1 != self.PickingItemIndex:
					textTail.ShowItemTextTail(self.PickingItemIndex)
			# END_OF_ADD_ALWAYS_SHOW_NAME

		## Show all name in the range

		# ADD_ALWAYS_SHOW_NAME
		if self.__IsShowName():
			textTail.ShowAllTextTail()
			self.PickingItemIndex = textTail.Pick(x, y)
		# END_OF_ADD_ALWAYS_SHOW_NAME

		textTail.UpdateShowingTextTail()
		textTail.ArrangeTextTail()
		if -1 != self.PickingItemIndex:
			textTail.SelectItemName(self.PickingItemIndex)

		grp.PopState()
		grp.SetInterfaceRenderState()

		textTail.Render()
		textTail.HideAllTextTail()

	def OnPressEscapeKey(self):
		if app.TARGET == app.GetCursor():
			app.SetCursor(app.NORMAL)

		elif True == mouseModule.mouseController.isAttached():
			mouseModule.mouseController.DeattachObject()

		else:
			self.interface.OpenSystemDialog()

		return True

	def OnIMEReturn(self):
		if app.IsPressed(app.DIK_LSHIFT):
			self.interface.OpenWhisperDialogWithoutTarget()
		else:
			self.interface.ToggleChat()
		return True

	def OnPressExitKey(self):
		self.interface.ToggleSystemDialog()
		return True

	## BINARY CALLBACK
	######################################################################################

	# WEDDING
	def BINARY_LoverInfo(self, name, lovePoint):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.OnAddLover(name, lovePoint)

		if self.affectBar:
			self.affectBar.GetItem("love").SetLoverInfo(name, lovePoint)

	def BINARY_UpdateLovePoint(self, lovePoint):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.OnUpdateLovePoint(lovePoint)

		if self.affectBar:
			self.affectBar.GetItem("love").OnUpdateLovePoint(lovePoint)
	# END_OF_WEDDING

	# QUEST_CONFIRM
	def BINARY_OnQuestConfirm(self, msg, timeout, pid):
		confirmDialog = uiCommon.QuestionDialogWithTimeLimit()
		confirmDialog.Open(msg, timeout)
		confirmDialog.SetAcceptEvent(lambda answer=True, pid=pid: net.SendQuestConfirmPacket(answer, pid) or self.confirmDialog.Hide())
		confirmDialog.SetCancelEvent(lambda answer=False, pid=pid: net.SendQuestConfirmPacket(answer, pid) or self.confirmDialog.Hide())
		self.confirmDialog = confirmDialog
	# END_OF_QUEST_CONFIRM

	def BINARY_AddItemToExchange(self, window, cell, slotIndex):
		# print "BINARY_AddItemToExchange %d %d %d" % (window, cell, slotIndex)
		eventManager.EventManager().send_event(uiExchange.EVENT_ADD_ITEM_TO_EXCHANGE, window, cell)

	# GIFT command
	def Gift_Show(self):
		self.interface.ShowGift()

	# CUBE
	def BINARY_Cube_Open(self, npcVNUM):
		self.currentCubeNPC = npcVNUM

		self.interface.OpenCubeWindow()


		if npcVNUM not in self.cubeInformation:
			net.SendChatPacket("/cube r_info")
		else:
			cubeInfoList = self.cubeInformation[npcVNUM]

			i = 0
			for cubeInfo in cubeInfoList:
				self.interface.wndCube.AddCubeResultItem(cubeInfo["vnum"], cubeInfo["count"])

				j = 0
				for materialList in cubeInfo["materialList"]:
					for materialInfo in materialList:
						itemVnum, itemCount = materialInfo
						self.interface.wndCube.AddMaterialInfo(i, j, itemVnum, itemCount)
					j = j + 1

				i = i + 1

			self.interface.wndCube.Refresh()

	def BINARY_Cube_Close(self):
		self.interface.CloseCubeWindow()

	def BINARY_Cube_UpdateInfo(self, gold, itemVnum, count):
		self.interface.UpdateCubeInfo(gold, itemVnum, count)

	def BINARY_Cube_Succeed(self, itemVnum, count):
		self.interface.SucceedCubeWork(itemVnum, count)
		pass

	def BINARY_Cube_Failed(self):
		self.interface.FailedCubeWork()
		pass

	def BINARY_Cube_ResultList(self, npcVNUM, listText):
		#print listText

		if npcVNUM == 0:
			npcVNUM = self.currentCubeNPC

		self.cubeInformation[npcVNUM] = []

		try:
			for eachInfoText in listText.split("/"):
				eachInfo = eachInfoText.split(",")
				itemVnum	= int(eachInfo[0])
				itemCount	= int(eachInfo[1])

				self.cubeInformation[npcVNUM].append({"vnum": itemVnum, "count": itemCount})
				self.interface.wndCube.AddCubeResultItem(itemVnum, itemCount)

			resultCount = len(self.cubeInformation[npcVNUM])
			requestCount = 7
			modCount = resultCount % requestCount
			splitCount = resultCount / requestCount
			for i in xrange(splitCount):
				#print("/cube r_info %d %d" % (i * requestCount, requestCount))
				net.SendChatPacket("/cube r_info %d %d" % (i * requestCount, requestCount))

			if 0 < modCount:
				#print("/cube r_info %d %d" % (splitCount * requestCount, modCount))
				net.SendChatPacket("/cube r_info %d %d" % (splitCount * requestCount, modCount))

		except RuntimeError, msg:
			dbg.TraceError(msg)
			return 0

		pass

	def BINARY_Cube_MaterialInfo(self, startIndex, listCount, listText):
		# Material Text Format : 125,1|126,2|127,2|123,5&555,5&555,4/120000
		try:
			#print listText

			if 3 > len(listText):
				dbg.TraceError("Wrong Cube Material Infomation")
				return 0



			eachResultList = listText.split("@")

			cubeInfo = self.cubeInformation[self.currentCubeNPC]

			itemIndex = 0
			for eachResultText in eachResultList:
				cubeInfo[startIndex + itemIndex]["materialList"] = [[], [], [], [], []]
				materialList = cubeInfo[startIndex + itemIndex]["materialList"]

				gold = 0
				splitResult = eachResultText.split("/")
				if 1 < len(splitResult):
					gold = int(splitResult[1])

				#print "splitResult : ", splitResult
				eachMaterialList = splitResult[0].split("&")

				i = 0
				for eachMaterialText in eachMaterialList:
					complicatedList = eachMaterialText.split("|")

					if 0 < len(complicatedList):
						for complicatedText in complicatedList:
							(itemVnum, itemCount) = complicatedText.split(",")
							itemVnum = int(itemVnum)
							itemCount = int(itemCount)
							self.interface.wndCube.AddMaterialInfo(itemIndex + startIndex, i, itemVnum, itemCount)

							materialList[i].append((itemVnum, itemCount))

					else:
						itemVnum, itemCount = eachMaterialText.split(",")
						itemVnum = int(itemVnum)
						itemCount = int(itemCount)
						self.interface.wndCube.AddMaterialInfo(itemIndex + startIndex, i, itemVnum, itemCount)

						materialList[i].append((itemVnum, itemCount))

					i = i + 1



				itemIndex = itemIndex + 1

			self.interface.wndCube.Refresh()


		except RuntimeError, msg:
			dbg.TraceError(msg)
			return 0

		pass

	# END_OF_CUBE

	def BINARY_Highlight_Item(self, inven_type, inven_pos):
		# @fixme003 (+if self.interface:)
		if self.interface:
			self.interface.Highligt_Item(inven_type, inven_pos)

	def BINARY_DragonSoulGiveQuilification(self):
		self.interface.DragonSoulGiveQuilification()

	def BINARY_DragonSoulRefineWindow_Open(self):
		self.interface.OpenDragonSoulRefineWindow()

	def BINARY_DragonSoulRefineWindow_RefineFail(self, reason, inven_type, inven_pos):
		self.interface.FailDragonSoulRefine(reason, inven_type, inven_pos)

	def BINARY_DragonSoulRefineWindow_RefineSucceed(self, inven_type, inven_pos):
		self.interface.SucceedDragonSoulRefine(inven_type, inven_pos)

	# END of DRAGON SOUL REFINE WINDOW

	def BINARY_SetBigMessage(self, message):
		self.interface.bigBoard.SetTip(message)

	def BINARY_SetTipMessage(self, message):
		self.interface.tipBoard.SetTip(message)

	def BINARY_SetFancyTipMessage(self, message, type):
		is_gm = type == chat.CHAT_TYPE_GAMEMASTER_NOTICE
		self.interface.fancyBoard.SetTip(message, is_gm)

	def BINARY_AppendNotifyMessage(self, type):
		if not type in localeInfo.NOTIFY_MESSAGE:
			return
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.NOTIFY_MESSAGE[type])

	def BINARY_Guild_EnterGuildArea(self, areaID):
		self.interface.BULID_EnterGuildArea(areaID)

	def BINARY_Guild_ExitGuildArea(self, areaID):
		self.interface.BULID_ExitGuildArea(areaID)

	def BINARY_GuildWar_OnSendDeclare(self, guildID):
		pass

	def BINARY_GuildWar_OnRecvDeclare(self, guildID, warType):
		mainCharacterName = player.GetMainCharacterName()
		masterName = guild.GetGuildMasterName()
		if mainCharacterName == masterName:
			self.__GuildWar_OpenAskDialog(guildID, warType)

	def BINARY_GuildWar_OnRecvPoint(self, gainGuildID, opponentGuildID, point):
		self.interface.OnRecvGuildWarPoint(gainGuildID, opponentGuildID, point)

	def BINARY_GuildWar_OnStart(self, guildSelf, guildOpp):
		self.interface.OnStartGuildWar(guildSelf, guildOpp)

		if background.GetCurrentMapName() in ("metin2_map_t3","metin2_map_t4"):
			self.interface.wndGameButton.SetWarMode(True)

	def BINARY_GuildWar_OnEnd(self, guildSelf, guildOpp):
		self.interface.OnEndGuildWar(guildSelf, guildOpp)

	def BINARY_BettingGuildWar_SetObserverMode(self, isEnable):
		self.interface.BINARY_SetObserverMode(isEnable)

	def BINARY_BettingGuildWar_UpdateObserverCount(self, observerCount):
		self.interface.wndMiniMap.UpdateObserverCount(observerCount)

	def __GuildWar_UpdateMemberCount(self, guildID1, memberCount1, guildID2, memberCount2, observerCount):
		guildID1 = int(guildID1)
		guildID2 = int(guildID2)
		memberCount1 = int(memberCount1)
		memberCount2 = int(memberCount2)
		observerCount = int(observerCount)

		self.interface.UpdateMemberCount(guildID1, memberCount1, guildID2, memberCount2)
		self.interface.wndMiniMap.UpdateObserverCount(observerCount)

	def __GuildWar_OpenAskDialog(self, guildID, warType):

		guildName = guild.GetGuildName(guildID)

		# REMOVED_GUILD_BUG_FIX
		if "Noname" == guildName:
			return
		# END_OF_REMOVED_GUILD_BUG_FIX

		import uiGuild
		questionDialog = uiGuild.AcceptGuildWarDialog()
		questionDialog.SAFE_SetAcceptEvent(self.__GuildWar_OnAccept)
		questionDialog.SAFE_SetCancelEvent(self.__GuildWar_OnDecline)
		questionDialog.Open(guildName, warType)

		self.guildWarQuestionDialog = questionDialog

	def __GuildWar_CloseAskDialog(self):
		self.guildWarQuestionDialog.Close()
		self.guildWarQuestionDialog = None

	def __GuildWar_OnAccept(self):

		guildName = self.guildWarQuestionDialog.GetGuildName()

		net.SendChatPacket("/war " + guildName)
		self.__GuildWar_CloseAskDialog()

		return 1

	def __GuildWar_OnDecline(self):

		guildName = self.guildWarQuestionDialog.GetGuildName()

		net.SendChatPacket("/nowar " + guildName)
		self.__GuildWar_CloseAskDialog()

		return 1
	## BINARY CALLBACK
	######################################################################################

	def __ServerCommand_Build(self):
		serverCommandList={
			"ConsoleEnable"			: self.__Console_Enable,
			"GameMaster"			: self.__GameMaster,
			"DayMode"				: self.__DayMode_Update,
			"PRESERVE_DayMode"		: self.__PRESERVE_DayMode_Update,
			"CloseRestartWindow"	: self.__RestartDialog_Close,
			"OpenPrivateShop"		: self.__PrivateShop_Open,
			"PartyHealReady"		: self.PartyHealReady,
			"ShowMeSafeboxPassword"	: self.AskSafeboxPassword,
			"CloseSafebox"			: self.CommandCloseSafebox,

			# ITEM_MALL
			"CloseMall"				: self.CommandCloseMall,
			"ShowMeMallPassword"	: self.AskMallPassword,
			"item_mall"				: self.__ItemMall_Open,
			# END_OF_ITEM_MALL

			"RefineSuceeded"		: self.RefineSuceededMessage,
			"RefineFailed"			: self.RefineFailedMessage,
			"xmas_snow"				: self.__XMasSnow_Enable,
			"xmas_boom"				: self.__XMasBoom_Enable,
			"xmas_song"				: self.__XMasSong_Enable,
			"xmas_tree"				: self.__XMasTree_Enable,
			"newyear_boom"			: self.__XMasBoom_Enable,
			"PartyRequest"			: self.__PartyRequestQuestion,
			"PartyRequestDenied"	: self.__PartyRequestDenied,
			"horse_state"			: self.__Horse_UpdateState,
			"hide_horse_state"		: self.__Horse_HideState,
			"WarUC"					: self.__GuildWar_UpdateMemberCount,
			"test_server"			: self.__EnableTestServerFlag,
			"mall"			: self.__InGameShop_Show,
			"SetGMFlag"				: self.__SetGMFlag,
			"OpenGMPanelWindow"		: self.__GMPanel_Open,
			"GMEquipChunk"			: self.__GMEquipChunk,
			"GMPanelLookupResult"	: self.__GMPanelLookupResult,
			"GMPanelSkillListResult"	: self.__GMPanelSkillListResult,
			"GMPanelGiveGoldResult"		: self.__GMPanelGiveGoldResult,
			"GMPanelGiveCashResult"		: self.__GMPanelGiveCashResult,
			"GMPanelSetHorsePointsResult"	: self.__GMPanelSetHorsePointsResult,
			"GMPanelSetRangeResult"		: self.__GMPanelSetRangeResult,
			"GMPanelSetStatResult"		: self.__GMPanelSetStatResult,
			"GMPanelSetSkillResult"		: self.__GMPanelSetSkillResult,
			"GMPanelCreateItemResult"	: self.__GMPanelCreateItemResult,
			"GMPanelPolyItemResult"	: self.__GMPanelPolyItemResult,
			"GMPanelItemListChunk"	: self.__GMPanelItemListChunk,
			"GMPanelWaypoint"	: self.__GMPanelWaypoint,
			"GMPanelAccountResult"	: self.__GMPanelAccountResult,
			"GMPanelAddGMResult"	: self.__GMPanelAddGMResult,
			"GMPanelSpawnResult"	: self.__GMPanelSpawnResult,
			"GMPanelSpawnMobResult"	: self.__GMPanelSpawnMobResult,
			"GMPanelSpawnBossResult"	: self.__GMPanelSpawnBossResult,
			"GMPanelSpawnMetinResult"	: self.__GMPanelSpawnMetinResult,
			"GMPanelRatesResult"	: self.__GMPanelRatesResult,
			"GMPanelSetRateResult"	: self.__GMPanelSetRateResult,
			"GMPanelRestartResult"	: self.__GMPanelRestartResult,
			"GMPanelAIWeightsResult"	: self.__GMPanelAIWeightsResult,
			"GMPanelSetAIWeightResult"	: self.__GMPanelSetAIWeightResult,

			"PlayerBotTitle"				: self.__PlayerBotTitle,
			"OpenPlayerbotAdminWindow"			: self.__PlayerbotAdmin_Open,
			"PlayerbotAdminStats"				: self.__PlayerbotAdmin_Stats,
			"PlayerbotAdminBotRow"				: self.__PlayerbotAdmin_BotRow,
			"PlayerbotAdminBotListEnd"			: self.__PlayerbotAdmin_BotListEnd,
			"PlayerbotAdminBotLogLine"			: self.__PlayerbotAdmin_BotLogLine,
			"PlayerbotAdminBotLogEnd"			: self.__PlayerbotAdmin_BotLogEnd,
			"PlayerbotAdminAchievementRow"		: self.__PlayerbotAdmin_AchievementRow,
			"PlayerbotAdminAchievementsEnd"	: self.__PlayerbotAdmin_AchievementsEnd,
			"PlayerbotOverhead"				: self.__PlayerbotAdmin_Overhead,
			"PlayerBotStatus"				: self.__PlayerBotStatus,
			"Top1Badge"							: self.__OnTop1Badge,
			"AutoHuntTarget"				: self.__AutoHuntTarget,
			"AutoHuntLoot"					: self.__AutoHuntLoot,

			# fishing
			"FishingGameStart": self.FishingGameStart,
			"FishingGameStop": self.FishingGameStop,
			"FishingGameCooldown": self.FishingGameCooldown,
			"FishingGameEvent": self.FishingGameEvent,

			# "Scal i uporzadkuj" (inventoryarrange.py)
			"InventoryArrangeResult"	: self.__InventoryArrangeResult,
			"SafeboxArrangeResult"	: self.__SafeboxArrangeResult,
			"SafeboxTransferResult"	: self.__SafeboxTransferResult,

			# WEDDING
			"lover_login"			: self.__LoginLover,
			"lover_logout"			: self.__LogoutLover,
			"lover_near"			: self.__LoverNear,
			"lover_far"				: self.__LoverFar,
			"lover_divorce"			: self.__LoverDivorce,
			"PlayMusic"				: self.__PlayMusic,
			# END_OF_WEDDING

			# PRIVATE_SHOP_PRICE_LIST
			"MyShopPriceList"		: self.__PrivateShop_PriceList,
			# END_OF_PRIVATE_SHOP_PRICE_LIST

			# item shop
			"itemshop_open": self.OpenItemShop,

			# crafting
			# "craft_request": self.CraftRequest,
			"craft_open": self.CraftOpen,
			"craft_avail": self.CraftAvail,
			"craft_recipe": self.CraftRecipe,
			"craft_update": self.CraftUpdate,

			"item_exchange_open": self.ItemExchangeOpen,
			"ToggleCameraMode": self.ToggleCameraMode,

			"PotionRechargeOpen": self.OpenPotionRecharge,

			"HideSpecialShop": self.HideSpecialShop,
			"UpdateSpecialShop": self.UpdateSpecialShop,

			"CloseBusyWindows": self.CloseBusyWindows,

			"maintenance": self.Maintenance,

			"SprintOnboarding": self.SprintOnboarding,
			"shop_edit": self.OfflineShopEdit,
	        "event": self.__ProcessServerEvent,
		}

		serverCommandList["SidekickInfo"] = self.__SidekickInfo
		serverCommandList["SidekickNames"] = self.__SidekickNames
		serverCommandList["SidekickGear"] = self.__SidekickGear
		serverCommandList["SidekickWindow"] = self.__SidekickWindow
		serverCommandList["AutoHuntOff"] = self.__AutoHuntOff
		serverCommandList["GlobalRankingWipe"] = self.__Global_Ranking__RecvWipe
		serverCommandList["GlobalRankingUpdatePacket"] = self.__Global_Ranking__RecvData
		serverCommandList["GlobalRankingUpdatePacketMyPos"] = self.__Global_Ranking__RecvSelfData
		serverCommandList["SidekickEqNone"] = self.__SidekickEqNone
		serverCommandList["SidekickEqBegin"] = self.__SidekickEqBegin
		serverCommandList["SidekickEqItem"] = self.__SidekickEqItem
		serverCommandList["SidekickEqEmpty"] = self.__SidekickEqEmpty
		serverCommandList["SidekickEqEnd"] = self.__SidekickEqEnd
		serverCommandList["SidekickEqResult"] = self.__SidekickEqResult
		serverCommandList["SidekickSkillBegin"] = self.__SidekickSkillBegin
		serverCommandList["SidekickSkill"] = self.__SidekickSkill
		serverCommandList["SidekickSkillEnd"] = self.__SidekickSkillEnd

		self.serverCommander=stringCommander.Analyzer()
		for serverCommandItem in serverCommandList.items():
			self.serverCommander.SAFE_RegisterCallBack(
				serverCommandItem[0], serverCommandItem[1]
			)

	def BINARY_ServerCommand_Run(self, line):
		try:
			#print " BINARY_ServerCommand_Run", line
			return self.serverCommander.Run(line)
		except RuntimeError, msg:
			dbg.TraceError(msg)
			return 0

	def __ProcessPreservedServerCommand(self):
		try:
			command = net.GetPreservedServerCommand()
			while command:
				print " __ProcessPreservedServerCommand", command
				self.serverCommander.Run(command)
				command = net.GetPreservedServerCommand()
		except RuntimeError, msg:
			dbg.TraceError(msg)
			return 0

	def OfflineShopEdit(self, strState):
		isEdit = int(strState)
		self.interface.offlineShopManage.SetEditMode(isEdit)

		if isEdit:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_EDIT_MODE_ON)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_EDIT_MODE_OFF)

	def SprintOnboarding(self):
		self.interface.ToggleCharacterWindow("SKILL")
		self.interface.wndCharacter.SetCenterPosition()
		self.interface.wndCharacter.ShowSprintOnboarding()

	def Maintenance(self, time_until, duration_seconds):
		time_until = int(time_until)
		duration_seconds = int(duration_seconds)

		if time_until > 0:
			self.interface.maintenanceDialog.Open(app.GetGlobalTimeStamp() + time_until, duration_seconds)
		else:
			self.interface.maintenanceDialog.Close()

	def UpdateSpecialShop(self):
		wndSpecialShop = self.interface.wndSpecialShop
		if wndSpecialShop.IsShow():
			if wndSpecialShop.itemToolTip:
				wndSpecialShop.itemToolTip.HideToolTip()

			if self.interface.wndPopupDialog:
				self.interface.wndPopupDialog.Close()

			self.interface.wndPopupDialog = uiCommon.PopupDialog()
			self.interface.wndPopupDialog.SetText(localeInfo.SPECIAL_SHOP_UPDATE_POPUP)
			self.interface.wndPopupDialog.SetAutoClose(3)
			self.interface.wndPopupDialog.Open()

	def HideSpecialShop(self):
		if self.interface.wndSpecialShop.IsShow():
			self.interface.wndSpecialShop.SoftClose()

	def CloseBusyWindows(self):
		interface = self.interface
		if interface.potionRechargeDialog.IsShow():
			interface.potionRechargeDialog.Close()

		if interface.wndItemShop.IsShow():
			interface.wndItemShop.Close()

		if interface.wndCrafting.IsShow():
			interface.wndCrafting.Close()

		if interface.wndSpecialShop.IsShow():
			interface.wndSpecialShop.Close()

		if interface.itemExchangeDialog.IsShow():
			interface.itemExchangeDialog.Close()

		if interface.offlineShopGuest.IsShow():
			interface.offlineShopGuest.Close()

		if interface.offlineShopManage.IsShow():
			interface.offlineShopManage.Close()

	def OpenPotionRecharge(self):
		self.interface.potionRechargeDialog.Open()

	def CraftUpdate(self, craft_item_vnum):
		self.interface.wndCrafting.OnCraftUpdate()

	# self.interface.wndCrafting.UpdateListItem(int(craft_item_vnum))

	def CraftAvail(self, strCraftVnum, strAvailData):
		craft_vnum = int(strCraftVnum)
		avail_data = strAvailData.split(",")

		if not constInfo.CRAFTING_DATA.has_key(craft_vnum):
			constInfo.CRAFTING_DATA[craft_vnum] = {}

		for data in avail_data:
			item_avail_data = data.split(":")
			craft_item_vnum = int(item_avail_data[0])
			detail_data = item_avail_data[1].split(".")
			is_available = int(detail_data[0])
			progress = int(detail_data[1])

			if not constInfo.CRAFTING_DATA[craft_vnum].has_key(craft_item_vnum):
				constInfo.CRAFTING_DATA[craft_vnum][craft_item_vnum] = {}

			craft_item_data = constInfo.CRAFTING_DATA[craft_vnum][craft_item_vnum]
			craft_item_data["is_available"] = is_available
			max_progress = craft_item_data["requirements"]["recipe_progress"][1]
			craft_item_data["requirements"]["recipe_progress"] = (progress, max_progress)

			constInfo.CRAFTING_DATA[craft_vnum][craft_item_vnum] = craft_item_data

	# def CraftRequest(self, strCraftVnum):
	# 	self.interface.wndCrafting.RequestOpen(int(strCraftVnum))

	def CraftOpen(self):
		self.interface.wndCrafting.Open()

	def CraftRecipe(self, strCraftVnum, strCraftItemsData):
		craft_vnum = int(strCraftVnum)
		craft_items_data = strCraftItemsData.split("|")

		self.interface.wndCrafting.current_crafting_vnum = int(strCraftVnum)

		if not constInfo.CRAFTING_DATA.has_key(craft_vnum):
			constInfo.CRAFTING_DATA[craft_vnum] = {}

		constInfo.CRAFTING_LOAD_DATA[craft_vnum] = True

		for strRecipeData in craft_items_data:
			recipe_data = strRecipeData.split(",")

			craft_item_vnum = int(recipe_data[0])
			item_vnum = int(recipe_data[1])
			count = int(recipe_data[2])
			price = int(recipe_data[3])
			chance = int(recipe_data[4])
			req_level = int(recipe_data[5])
			req_progress = int(recipe_data[6])
			strMaterialData = recipe_data[7]
			material_data = strMaterialData.split(".")
			# material_tuple = tuple(map(int, material_data))
			material_list = []
			for i in range(0, len(material_data), 2):
				material_vnum_str_data = material_data[i].split("-")
				material_vnum = 0
				if len(material_vnum_str_data) > 1:
					material_vnum_data = []
					for vnum in material_vnum_str_data:
						material_vnum_data.append(int(vnum))
					material_vnum = tuple(material_vnum_data)
				else:
					material_vnum = int(material_vnum_str_data[0])

				material_count = int(material_data[i + 1])
				material_list.append(material_vnum)
				material_list.append(material_count)

			material_tuple = tuple(material_list)

			if not constInfo.CRAFTING_DATA[craft_vnum].has_key(craft_item_vnum):
				constInfo.CRAFTING_DATA[craft_vnum][craft_item_vnum] = {}

			data = constInfo.CRAFTING_DATA[craft_vnum][craft_item_vnum]
			data["item_vnum"] = item_vnum
			data["count"] = count
			data["price"] = price
			data["chance"] = chance
			data["recipe"] = material_tuple
			data["requirements"] = {
				"recipe_progress": (0, req_progress),
				"min_level": req_level,
			}
			print "dodaje nowy crafting %d item %d" % (craft_vnum, craft_item_vnum)
			constInfo.CRAFTING_DATA[craft_vnum][craft_item_vnum] = data

	def FishingGameStart(self, bar_height, player_pos, start_delay):
		app.SetWindowDisplay(True)
		eventManager.EventManager().send_event(uiFishing.EVENT_FISHING_GAME_START, int(bar_height), int(player_pos),
											   int(start_delay))

	def FishingGameStop(self, isWin):
		app.SetWindowDisplay(False)
		eventManager.EventManager().send_event(uiFishing.EVENT_FISHING_GAME_STOP, True if int(isWin) == 1 else False)

	def FishingGameEvent(self, event_type, event_duration):
		eventManager.EventManager().send_event(uiFishing.EVENT_FISHING_GAME_EVENT, int(event_type), int(event_duration))

	def FishingGameCooldown(self, leftTime):
		self.interface.fishingGameDialog.SetCooldown(int(leftTime))

	def OpenItemShop(self):
		self.interface.wndItemShop.Open()

	def ToggleCameraMode(self):
		self.__SetFreeCamera()

	def ItemExchangeOpen(self, result_vnum, price, chance, exchange_vnum):
		self.interface.itemExchangeDialog.resultVnum = int(result_vnum)
		self.interface.itemExchangeDialog.costPerResultItem = int(price)
		self.interface.itemExchangeDialog.exchangeVnum = int(exchange_vnum)
		self.interface.itemExchangeDialog.chance = int(chance)
		self.interface.itemExchangeDialog.Open()

	def PartyHealReady(self):
		self.interface.PartyHealReady()

	def AskSafeboxPassword(self):
		self.interface.AskSafeboxPassword()

	# ITEM_MALL
	def AskMallPassword(self):
		self.interface.AskMallPassword()

	def __ItemMall_Open(self):
		self.interface.OpenItemMall();

	def CommandCloseMall(self):
		self.interface.CommandCloseMall()
	# END_OF_ITEM_MALL

	def RefineSuceededMessage(self):
		snd.PlaySound("sound/ui/make_soket.wav")
		self.PopupMessage(localeInfo.REFINE_SUCCESS)

	def RefineFailedMessage(self):
		snd.PlaySound("sound/ui/jaeryun_fail.wav")
		self.PopupMessage(localeInfo.REFINE_FAILURE)

	def CommandCloseSafebox(self):
		self.interface.CommandCloseSafebox()

	# PRIVATE_SHOP_PRICE_LIST
	def __PrivateShop_PriceList(self, itemVNum, itemPrice):
		pass
		# uiPrivateShopBuilder.SetPrivateShopItemPrice(itemVNum, itemPrice)
	# END_OF_PRIVATE_SHOP_PRICE_LIST

	def __Horse_HideState(self):
		self.affectBar.GetItem("horse").SetState(0, 0, 0)
		self.affectBar.ArrangeAffectImage()

		constInfo.IS_HORSE_SUMMONED = False
		eventManager.EventManager().send_event(eventManager.EVENT_HORSE_SUMMON, False)

	def __Horse_UpdateState(self, level, health, battery):
		self.affectBar.GetItem("horse").SetState(int(level), int(health), int(battery))
		self.affectBar.ArrangeAffectImage()

		constInfo.IS_HORSE_SUMMONED = True
		eventManager.EventManager().send_event(eventManager.EVENT_HORSE_SUMMON, True)

	def __IsXMasMap(self):
		mapDict = ( "metin2_map_n_flame_01",
					"metin2_map_n_desert_01",
					"metin2_map_spiderdungeon",
					"metin2_map_deviltower1", )

		if background.GetCurrentMapName() in mapDict:
			return False

		return True

	def __XMasSnow_Enable(self, mode):

		self.__XMasSong_Enable(mode)

		if "1"==mode:

			if not self.__IsXMasMap():
				return

			print "XMAS_SNOW ON"
			background.EnableSnow(1)

		else:
			print "XMAS_SNOW OFF"
			background.EnableSnow(0)

	def __XMasBoom_Enable(self, mode):
		if "1"==mode:

			if not self.__IsXMasMap():
				return

			print "XMAS_BOOM ON"
			self.__DayMode_Update("dark")
			self.enableXMasBoom = True
			self.startTimeXMasBoom = app.GetTime()
		else:
			print "XMAS_BOOM OFF"
			self.__DayMode_Update("light")
			self.enableXMasBoom = False

	def __XMasTree_Enable(self, grade):

		print "XMAS_TREE ", grade
		background.SetXMasTree(int(grade))

	def __XMasSong_Enable(self, mode):
		if "1"==mode:
			print "XMAS_SONG ON"

			XMAS_BGM = "xmas.mp3"

			if app.IsExistFile("BGM/" + XMAS_BGM)==1:
				if musicInfo.fieldMusic != "":
					snd.FadeOutMusic("BGM/" + musicInfo.fieldMusic)

				musicInfo.fieldMusic=XMAS_BGM
				snd.FadeInMusic("BGM/" + musicInfo.fieldMusic)

		else:
			print "XMAS_SONG OFF"

			if musicInfo.fieldMusic != "":
				snd.FadeOutMusic("BGM/" + musicInfo.fieldMusic)

			musicInfo.fieldMusic=musicInfo.METIN2THEMA
			snd.FadeInMusic("BGM/" + musicInfo.fieldMusic)

	def __RestartDialog_Close(self):
		self.interface.CloseRestartDialog()

	def __Console_Enable(self):
		constInfo.CONSOLE_ENABLE = True
		self.consoleEnable = True
		app.EnableSpecialCameraMode()
		ui.EnablePaste(True)

	def __GameMaster(self, admin_api):
		constInfo.ADMIN_API = admin_api
		constInfo.IS_GAMEMASTER = True

	def BINARY_Actor_Disappear(self, vid):
		self.interface.DisappearPrivateShop(vid)
		self.interface.DisappearReputationBar(vid)
		if vid == self.targetBoard.GetTargetVID():
			self.CloseTargetBoard()

	def OnMyShopBuy(self, vnum, count, price):
		if vnum > 1:
			item.SelectItem(vnum)
			message = localeInfo.MYSHOP_BUY % (count, item.GetItemName(), localeInfo.NumberToMoneyString(price))
			self.OnRecvWhisper(chat.WHISPER_TYPE_MYSHOP, "[Sklep]", message)

	## PrivateShop
	def __PrivateShop_Open(self, tax):
		self.interface.offlineShopBuilder.Open(int(tax))

	def BINARY_PrivateShop_Appear(self, vid, text):
		self.interface.AppearPrivateShop(vid, text)

	def BINARY_OfflineShop_Appear(self, vid, text):
		self.interface.AppearPrivateShop(vid, text, True)

	def BINARY_OfflineShop_Disappear(self, vid):
		self.interface.DisappearPrivateShop(vid, True)

	def BINARY_OfflineShop_ToggleVisibility(self, vid, is_visible):
		self.interface.ToggleOfflineShopVisibility(vid, is_visible)

	def BINARY_NpcInfoBoard_Reputation(self, vid, fraction):
		self.interface.AppearReputationBar(vid, fraction)

	## NightMode
	def __CanSetNightMode(self):
		mapDict = (
			"metin_icedungeon",
			"metin2_map_devilscatacomb",
			"metin2_map_deviltower1",
			"metin2_map_labirynth",
			"metin2_map_monkeydungeon",
			"metin2_map_monkeydungeon_02",
			"metin2_map_monkeydungeon_03",
			"metin2_map_n_flame_01",
			"metin2_map_skipia_bossdungeon",
			"metin2_map_skipia_dungeon_01",
			"metin2_map_skipia_dungeon_02",
			"metin2_map_skipia_dungeon_boss",
			"metin2_map_spider_bossdungeon",
			"metin2_map_spiderdungeon",
			"metin2_map_spiderdungeon_02",
			"metin2_map_studio",
			"metin2_map_t3",
			"metin2_map_t4",
			"metin2_map_trent",
			"metin2_map_wl_pass",
		)

		if background.GetCurrentMapName() in mapDict:
			return False

		return True

	def __UpdateNightMode(self):
		if not self.is_auto_night:
			return

		try:
			now_time = datetime.datetime.now()
		except ValueError as e:
			return

		if 22 <= now_time.hour <= 23 or 0 <= now_time.hour < 6:
			self.__SetNightMode(True)
		else:
			self.__SetNightMode(False)

	def __RefreshNightMode(self):
		mode = systemSetting.GetNightMode()
		self.is_auto_night = mode == 2
		if mode == 1:
			self.__SetNightMode(True)
		elif mode == 2:
			self.__UpdateNightMode()
		else:
			self.__SetNightMode(False)

	def __OnChangeNightMode(self, mode):
		self.__RefreshNightMode()

	def __SetNightMode(self, isNight):
		if self.is_night_environment == isNight:
			return

		if not isNight:
			background.SetEnvironmentData(0)
			self.is_night_environment = False
			return

		if not self.__CanSetNightMode():
			return

		current_map_name = background.GetCurrentMapName()
		map_night_environment = {
			"map_a2": "a2_night",
			"metin2_map_a3": "a3_night",
			"metin2_map_b3": "a3_night",
			"metin2_map_c3": "a3_night",
			"map_n_snowm_01": "snow_night",
			"metin2_map_n_desert_01": "desert_night",
			"metin2_map_milgyo": "milgyo_night",
			"metin2_map_nusluck01": "nusluck_night",
			"metin2_map_trent02": "trent02_night",
		}

		environment_file = "moonlight04"
		if map_night_environment.has_key(current_map_name):
			environment_file = map_night_environment[current_map_name]

		background.RegisterEnvironmentData(1, "d:/ymir work/environment/%s.msenv" % environment_file)
		background.SetEnvironmentData(1)
		self.is_night_environment = True

	## DayMode
	def __PRESERVE_DayMode_Update(self, mode):
		if "light"==mode:
			background.SetEnvironmentData(0)
		elif "dark"==mode:

			if not self.__IsXMasMap():
				return

			background.RegisterEnvironmentData(1, constInfo.ENVIRONMENT_NIGHT)
			background.SetEnvironmentData(1)

	def __DayMode_Update(self, mode):
		if "light"==mode:
			self.curtain.SAFE_FadeOut(self.__DayMode_OnCompleteChangeToLight)
		elif "dark"==mode:

			if not self.__IsXMasMap():
				return

			self.curtain.SAFE_FadeOut(self.__DayMode_OnCompleteChangeToDark)

	def __DayMode_OnCompleteChangeToLight(self):
		background.SetEnvironmentData(0)
		self.curtain.FadeIn()

	def __DayMode_OnCompleteChangeToDark(self):
		background.RegisterEnvironmentData(1, constInfo.ENVIRONMENT_NIGHT)
		background.SetEnvironmentData(1)
		self.curtain.FadeIn()

	## XMasBoom
	def __XMasBoom_Update(self):

		self.BOOM_DATA_LIST = ( (2, 5), (5, 2), (7, 3), (10, 3), (20, 5) )
		if self.indexXMasBoom >= len(self.BOOM_DATA_LIST):
			return

		boomTime = self.BOOM_DATA_LIST[self.indexXMasBoom][0]
		boomCount = self.BOOM_DATA_LIST[self.indexXMasBoom][1]

		if app.GetTime() - self.startTimeXMasBoom > boomTime:

			self.indexXMasBoom += 1

			for i in xrange(boomCount):
				self.__XMasBoom_Boom()

	def __XMasBoom_Boom(self):
		x, y, z = player.GetMainCharacterPosition()
		randX = app.GetRandom(-150, 150)
		randY = app.GetRandom(-150, 150)

		snd.PlaySound3D(x+randX, -y+randY, z, "sound/common/etc/salute.mp3")

	def __PartyRequestQuestion(self, vid):
		vid = int(vid)
		partyRequestQuestionDialog = uiCommon.QuestionDialog()
		partyRequestQuestionDialog.SetText(chr.GetNameByVID(vid) + localeInfo.PARTY_DO_YOU_ACCEPT)
		partyRequestQuestionDialog.SetAcceptText(localeInfo.UI_ACCEPT)
		partyRequestQuestionDialog.SetCancelText(localeInfo.UI_DENY)
		partyRequestQuestionDialog.SetAcceptEvent(lambda arg=True: self.__AnswerPartyRequest(arg))
		partyRequestQuestionDialog.SetCancelEvent(lambda arg=False: self.__AnswerPartyRequest(arg))
		partyRequestQuestionDialog.Open()
		partyRequestQuestionDialog.vid = vid
		self.partyRequestQuestionDialog = partyRequestQuestionDialog

	def __AnswerPartyRequest(self, answer):
		if not self.partyRequestQuestionDialog:
			return

		vid = self.partyRequestQuestionDialog.vid

		if answer:
			net.SendChatPacket("/party_request_accept " + str(vid))
		else:
			net.SendChatPacket("/party_request_deny " + str(vid))

		self.partyRequestQuestionDialog.Close()
		self.partyRequestQuestionDialog = None

	def __PartyRequestDenied(self):
		self.PopupMessage(localeInfo.PARTY_REQUEST_DENIED)

	def __EnableTestServerFlag(self):
		app.EnableTestServerFlag()

	# ---- Panel GM (F9) / admin botow (F10): odbior odpowiedzi serwera ----
	def __SetGMFlag(self):
		# Sent once at login, only to characters the server itself already
		# knows are GM (CHARACTER::GetGMLevel() > 0) - the client never
		# decides this on its own, it only learns it. F9 itself no longer
		# depends on this (it round-trips /gmpanel_open live instead - this
		# one-shot push, deferred 3s after login, turned out unreliable as
		# a cached gate). Still gates the bot overhead-tail dymek (see
		# __PlayerbotAdmin_Overhead below), which has no per-press
		# round-trip of its own to lean on instead.
		constInfo.IsGM = True

		# Odpowiedz przychodzi asynchronicznie. Bez tego okno celu otwarte
		# wczesniej zostawalo bez przyciskow GM az do wskazania nastepnej
		# postaci - a to wygladalo, jakby pierwsze kliniecie nie dzialalo.
		if self.targetBoard and self.targetBoard.IsShowButton():
			self.targetBoard.RefreshButton()

	def __GMPanel_Open(self):
		self.interface.ToggleGMPanelWindow()

	def __GMEquipChunk(self, vid, isLast, data):
		self.interface.SetGMEquipChunk(vid, isLast, data)

	def __GMPanelLookupResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetLookupResult(data)

	def __GMPanelSkillListResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetSkillListResult(data)

	def __GMPanelGiveGoldResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetGiveGoldResult(data)

	def __GMPanelGiveCashResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetGiveCashResult(data)

	def __GMPanelSetHorsePointsResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetHorsePointsResult(data)

	def __GMPanelSetRangeResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetRangeResult(data)

	def __GMPanelSetStatResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetStatResult(data)

	def __GMPanelSetSkillResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetSetSkillResult(data)

	def __GMPanelCreateItemResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetCreateItemResult(data)

	def __GMPanelPolyItemResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetPolyItemResult(data)

	def __GMPanelItemListChunk(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetItemListChunk(data)

	def __GMPanelWaypoint(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetWaypoint(data)

	def __GMPanelAccountResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetAccountResult(data)

	def __GMPanelAddGMResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetAddGMResult(data)

	def __GMPanelSpawnResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetSpawnResult(data)

	def __GMPanelSpawnMobResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetSpawnMobResult(data)

	def __GMPanelSpawnBossResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetSpawnBossResult(data)

	def __GMPanelSpawnMetinResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetSpawnMetinResult(data)

	def __GMPanelRatesResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetRatesResult(data)

	def __GMPanelSetRateResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetRateSaveResult(data)

	def __GMPanelRestartResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetRestartServerResult(data)

	def __GMPanelAIWeightsResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetAIWeightsResult(data)

	def __GMPanelSetAIWeightResult(self, data):
		if self.interface.wndGMPanel:
			self.interface.wndGMPanel.SetAIWeightResult(data)

	def __PlayerbotAdmin_Open(self):
		self.interface.OpenPlayerbotAdminWindow()

	def __PlayerbotAdmin_Stats(self, total, inParty, stalls):
		if self.interface.wndPlayerbotAdmin:
			self.interface.wndPlayerbotAdmin.OnStats(total, inParty, stalls)

	def __PlayerbotAdmin_BotRow(self, pid, level, empire, x, y, name):
		if self.interface.wndPlayerbotAdmin:
			self.interface.wndPlayerbotAdmin.OnBotRow(pid, level, empire, x, y, name)

	def __PlayerbotAdmin_BotListEnd(self):
		if self.interface.wndPlayerbotAdmin:
			self.interface.wndPlayerbotAdmin.OnBotListEnd()

	def __PlayerbotAdmin_BotLogLine(self, pid, text):
		if self.interface.wndPlayerbotAdmin:
			self.interface.wndPlayerbotAdmin.OnBotLogLine(pid, text)

	def __PlayerbotAdmin_BotLogEnd(self, pid):
		if self.interface.wndPlayerbotAdmin:
			self.interface.wndPlayerbotAdmin.OnBotLogEnd(pid)

	def __PlayerbotAdmin_AchievementRow(self, id, pid, name):
		if self.interface.wndPlayerbotAdmin:
			self.interface.wndPlayerbotAdmin.OnAchievementRow(id, pid, name)

	def __PlayerBotTitle(self, vid="0", personality="-1", *rest):
		# A bot's personality where a player's alignment title stands
		# (playerbot_status_tail.py; ManagePlayerBotPersonalityTitle on the server).
		import playerbot_status_tail
		if playerbot_status_tail.show_title(vid, personality) and not getattr(self, "playerbotTitleKeeper", None):
			self.playerbotTitleKeeper = playerbot_status_tail.GetTitleKeeper()
			self.RegisterUpdatable(self.playerbotTitleKeeper)

	def __PlayerbotAdmin_AchievementsEnd(self):
		if self.interface.wndPlayerbotAdmin:
			self.interface.wndPlayerbotAdmin.OnAchievementsEnd()

	def __PlayerbotAdmin_Overhead(self, vid, wire):
		# Broadcast to everyone nearby the bot (see SendPlayerBotOverheadTail,
		# playerbot_status.h) - gated here, not server-side, so non-GM clients
		# just drop the packet instead of building the overhead panel widget.
		if constInfo.IsGM:
			if self.interface.wndPlayerbotAdmin:
				self.interface.wndPlayerbotAdmin.OnOverheadTail(vid, wire)

	def __PlayerBotStatus(self, vid="0", encodedText="", *rest):
		# A bot's status as a text tail and nothing in the chat history
		# (playerbot_status_tail.py; SendPlayerBotOverheadChat on the server).
		import playerbot_status_tail
		playerbot_status_tail.show(vid, encodedText, *rest)

	# Same transport as PlayerbotOverhead above (SendPlayerBotOverheadTail),
	# but not GM-gated - the "Top1" badge over the server's current highest
	# level character is meant for everyone (top1_badge_event,
	# playerbot_manager.cpp).
	def __OnTop1Badge(self, vid):
		if self.interface.wndTop1Badge:
			self.interface.wndTop1Badge.Refresh(vid)

	def __SafeboxArrangeResult(self, code="0", moved="0", merged="0", units="0", *rest):
		import safeboxtransfer
		safeboxtransfer.OnArrangeResult(code, moved, merged, units)

	def __SafeboxTransferResult(self, op="0", code="0", units="0", *rest):
		import safeboxtransfer
		safeboxtransfer.OnTransferResult(op, code, units)

	def __InventoryArrangeResult(self, code="0", moved="0", merged="0", units="0", *rest):
		import inventoryarrange
		inventoryarrange.OnResult(code, moved, merged, units)

	def __InGameShop_Show(self, url):
		if constInfo.IN_GAME_SHOP_ENABLE:
			self.interface.OpenWebWindow(url)

	# WEDDING
	def __LoginLover(self, is_marriage_premium):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.OnLoginLover()

		if self.affectBar:
			self.affectBar.GetItem("love").Show()
			self.affectBar.ArrangeAffectImage()

		if int(is_marriage_premium) > 0:
			constInfo.IS_MARRIAGE_PREMIUM = True

	def __LogoutLover(self):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.OnLogoutLover()

		if self.affectBar:
			self.affectBar.GetItem("love").Hide()
			self.affectBar.ArrangeAffectImage()

		constInfo.IS_MARRIAGE_PREMIUM = False

	def __LoverNear(self):
		if self.affectBar:
			self.affectBar.GetItem("love").Show()
			self.affectBar.ArrangeAffectImage()

	def __LoverFar(self):
		if self.affectBar:
			self.affectBar.GetItem("love").Hide()
			self.affectBar.ArrangeAffectImage()

	def __LoverDivorce(self):
		if self.interface.wndMessenger:
			self.interface.wndMessenger.ClearLoverInfo()
		if self.affectBar:
			self.affectBar.GetItem("love").Hide()
			self.affectBar.ArrangeAffectImage()

		constInfo.IS_MARRIAGE_PREMIUM = False

	def __PlayMusic(self, flag, filename):
		flag = int(flag)
		if flag:
			snd.FadeOutAllMusic()
			musicInfo.SaveLastPlayFieldMusic()
			snd.FadeInMusic("BGM/" + filename)
		else:
			snd.FadeOutAllMusic()
			musicInfo.LoadLastPlayFieldMusic()
			snd.FadeInMusic("BGM/" + musicInfo.fieldMusic)
	# END_OF_WEDDING

	if app.ENABLE_ACCE_COSTUME_SYSTEM:
		def ActAcce(self, iAct, bWindow):
			if self.interface:
				self.interface.ActAcce(iAct, bWindow)

		def AlertAcce(self, bWindow):
			snd.PlaySound("sound/ui/make_soket.wav")
			if bWindow:
				self.PopupMessage(localeInfo.ACCE_DEL_SERVEITEM)
			else:
				self.PopupMessage(localeInfo.ACCE_DEL_ABSORDITEM)

	def __ProcessServerEvent(self, *args):
		"""
		Process events received from the server and dispatch them to the EventManager.
		
		Args:
			*args: Arguments from the server. First arg is the event name,
				followed by positional or keyword arguments (with format key=value).
		
		Returns:
			bool: True if event was processed successfully, False otherwise.
		"""
		if not args:
			return False
		
		try:
			event_name = args[0]
		
			positional_args = []
			keyword_args = {}
			
			# Parse and convert arguments from strings to appropriate Python types
			for arg in args[1:]:
				try:
					if "=" in arg:
						# Handle keyword arguments (key=value format)
						key, value = arg.split("=", 1)
						keyword_args[key] = self.__ConvertArgValue(value)
					else:
						# Handle positional arguments
						positional_args.append(self.__ConvertArgValue(arg))
				except Exception as e:
					dbg.TraceError("Error parsing argument '{}': {}".format(arg, e))
					# Continue processing other arguments even if one fails
			
			# Import event manager and dispatch the event
			import eventManager 
			eventManager.EventManager().send_event(event_name, *positional_args, **keyword_args)
			
		except Exception as e:
			# Catch-all for any unexpected errors
			import dbg
			import traceback
			error_msg = traceback.format_exc()
			dbg.TraceError("Error processing server event: {}".format(error_msg))
			return False

	def __ConvertArgValue(self, value_str):
		"""
		Convert a string value to an appropriate Python type.
		
		Args:
			value_str (str): The string value to convert
			
		Returns:
			The converted value (int, float, bool, or str)
		"""
		# Handle None/null values
		if value_str.lower() in ("none", "null"):
			return None
			
		# Handle boolean values
		if value_str.lower() == "true":
			return True
		if value_str.lower() == "false":
			return False
		
		# Handle numeric values
		try:
			# Try to convert to integer
			if value_str.isdigit() or (value_str[0] == '-' and value_str[1:].isdigit()):
				return int(value_str)
			
			# Try to convert to float
			if '.' in value_str:
				# Ensure there's only one decimal point and all other chars are digits
				parts = value_str.split('.')
				if len(parts) == 2 and (parts[0].isdigit() or (parts[0][0] == '-' and parts[0][1:].isdigit())) and parts[1].isdigit():
					return float(value_str)
		except (ValueError, IndexError):
			pass
		
		# If all else fails, return as string
		return value_str
	
	def __Global_Ranking__RecvWipe(self, iStart, iEnd):
		self.interface.GetInterfaceWindow("GlobalRankingsManager").RefreshHighscore(int(iStart), int(iEnd))

	def __Global_Ranking__RecvData(self, iCategory, iNum, sName, iEmpire, lScore):
		if lScore.find(".00") != -1:
			lScore = int(float(lScore))
		else:
			lScore = float(lScore)

		self.interface.GetInterfaceWindow("GlobalRankingsManager").UpdateRankingData(int(iCategory), int(iNum), sName, int(iEmpire), lScore)

	def __Global_Ranking__RecvSelfData(self, iCategory, iPos, sName, iEmpire, lScore):
		if lScore.find(".00") != -1:
			lScore = int(float(lScore))
		else:
			lScore = float(lScore)

		self.interface.GetInterfaceWindow("GlobalRankingsManager").UpdateRankingData_Self(int(iCategory), int(iPos), sName, int(iEmpire), lScore)
