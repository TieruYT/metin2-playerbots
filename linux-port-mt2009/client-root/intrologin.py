import dbg
import app
import net
import ui
import ime
import snd
import wndMgr
import musicInfo
import serverInfo
import systemSetting
import ServerStateChecker
import localeInfo
import constInfo
import uiCommon
import time
import serverCommandParser
import ime
import uiScriptLocale
import localeinfo_point
import hash_utils
import file_utils
import simplejson as json
import utils
import gameRules
import gameSettings
import configMain
import uiToolTip
import eventManager
import autologin

LOGIN_DELAY_SEC = 0.0
SKIP_LOGIN_PHASE = False
SKIP_LOGIN_PHASE_SUPPORT_CHANNEL = False

MAX_CREDENTIALS_SAVE = 3
SAVED_CREDENTIALS = {}

EVENT_TRY_CONNECT = "EVENT_TRY_CONNECT"
EVENT_REQUEST_STATE_CHECK = "EVENT_REQUEST_STATE_CHECK"

def IsLoginDelay():
	global LOGIN_DELAY_SEC
	if LOGIN_DELAY_SEC > 0.0:
		return True
	else:
		return False

def GetLoginDelay():
	global LOGIN_DELAY_SEC
	return LOGIN_DELAY_SEC

app.SetGuildMarkPath("test")

def get_user_unique_key():
	return hash_utils.generate_key(hash_utils.get_mac_address())

def encrypt_password(user_password):
	return hash_utils.simple_encrypt(get_user_unique_key(), user_password)

def decrypt_password(password_hash):
	return hash_utils.simple_decrypt(get_user_unique_key(), password_hash)

def load_credentials():
	stringData = file_utils.GetFileContent("cache/credentials.json")
	if len(stringData) > 0:
		global SAVED_CREDENTIALS
		SAVED_CREDENTIALS = json.loads(stringData)

def save_credentials(index, login, password):
	global SAVED_CREDENTIALS
	SAVED_CREDENTIALS[index] = {}
	SAVED_CREDENTIALS[index]["login"] = login
	SAVED_CREDENTIALS[index]["password"] = encrypt_password(password)
	save_credentials_file()

def save_credentials_file():
	file_utils.WriteFile("cache/credentials.json", json.dumps(SAVED_CREDENTIALS))

class SaveCredentialsItem(ui.ThinBoard):
	def __init__(self, index, parent, parent_window):
		ui.ThinBoard.__init__(self)
		self.SetParent(parent_window)
		self.parent = parent
		self.index = index
		self.Setup(index)

	def __del__(self):
		ui.ThinBoard.__del__(self)

	def Destroy(self):
		self.parent = None
		self.tooltip = None
		self.Hide()

	def SetToolTip(self, tooltip):
		self.tooltip = tooltip

	def Setup(self, index):
		self.SetSize(208, 30)
		self.SetPosition(0, 35 * index)

		login_label = ui.TextLine()
		login_label.SetParent(self)
		login_label.SetOutline(True)
		login_label.SetBold(True)
		login_label.SetText("123456789012")
		login_label.SetWindowVerticalAlignCenter()
		login_label.SetVerticalAlignCenter()
		login_label.SetPosition(12, 0)
		login_label.Show()
		self.login_label = login_label

		primary_button = ui.Button()
		primary_button.SetParent(self)
		primary_button.SetWindowHorizontalAlignRight()
		primary_button.SetWindowVerticalAlignCenter()
		primary_button.SetUpVisual("d:/ymir work/ui/public/small_button_01.sub")
		primary_button.SetOverVisual("d:/ymir work/ui/public/small_button_02.sub")
		primary_button.SetDownVisual("d:/ymir work/ui/public/small_button_03.sub")
		primary_button.SAFE_SetStringEvent("MOUSE_OVER_IN", self.__MouseOverIn)
		primary_button.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.__MouseOverOut)
		primary_button.SetText(uiScriptLocale.CREDENTIALS_SAVE)
		primary_button.SetPosition(95, 2)
		primary_button.Show()
		self.primary_button = primary_button

		delete_button = ui.Button()
		delete_button.SetParent(self)
		delete_button.SetWindowHorizontalAlignRight()
		delete_button.SetWindowVerticalAlignCenter()
		delete_button.SetUpVisual("d:/ymir work/ui/public/small_button_01.sub")
		delete_button.SetOverVisual("d:/ymir work/ui/public/small_button_02.sub")
		delete_button.SetDownVisual("d:/ymir work/ui/public/small_button_03.sub")
		delete_button.SetText(uiScriptLocale.CREDENTIALS_DELETE)
		delete_button.SetPosition(52, 2)
		delete_button.Show()
		self.delete_button = delete_button

		self.primary_button.SAFE_SetEvent(self.__OnPrimaryButtonClick)
		self.delete_button.SAFE_SetEvent(self.__OnDeleteButtonClick)
		self.Refresh()

	def __MouseOverIn(self):
		if self.tooltip:
			self.tooltip.ClearToolTip()
			self.tooltip.AppendTextLine(localeInfo.SAVE_ACCOUNT_LOAD_SHORTCUT_TOOLTIP % (self.index+1))
			self.tooltip.ShowToolTip()

	def __MouseOverOut(self):
		if self.tooltip:
			self.tooltip.HideToolTip()

	def IsSaved(self):
		return SAVED_CREDENTIALS.has_key(str(self.index))

	def Save(self):
		login = self.parent.idEditLine.GetText()
		pwd = self.parent.pwdEditLine.GetText()
		if len(login) < 1 or len(pwd) < 1:
			self.parent.PopupNotifyMessage(localeInfo.CREDENTIALS_FILL)
			return
		save_credentials(str(self.index), self.parent.idEditLine.GetText(), self.parent.pwdEditLine.GetText())
		self.Refresh()

	def Load(self):
		data = SAVED_CREDENTIALS[str(self.index)]
		self.parent.idEditLine.SetText(str(data["login"]))
		self.parent.pwdEditLine.SetText(decrypt_password(str(data["password"])))
		self.parent.OnClickLoginButton()

	def __OnPrimaryButtonClick(self):
		if self.IsSaved():
			self.Load()
		else:
			self.Save()

	def __OnDeleteButtonClick(self):
		SAVED_CREDENTIALS.pop(str(self.index))
		self.Refresh()
		save_credentials_file()

	def Refresh(self):
		if self.IsSaved():
			self.primary_button.SetText(uiScriptLocale.CREDENTIALS_LOAD)
			self.delete_button.SetUp()
			self.delete_button.Enable()
			self.login_label.SetText(str(SAVED_CREDENTIALS[str(self.index)]["login"]))
		else:
			self.primary_button.SetText(uiScriptLocale.CREDENTIALS_SAVE)
			self.delete_button.Down()
			self.delete_button.Disable()
			self.login_label.SetText("...")

class ConnectingDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__LoadDialog()
		self.eventTimeOver = lambda *arg: None
		self.eventExit = lambda *arg: None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/ConnectingDialog.py")

			self.board = self.GetChild("board")
			self.message = self.GetChild("message")
			self.countdownMessage = self.GetChild("countdown_message")

		except:
			import exception
			exception.Abort("ConnectingDialog.LoadDialog.BindObject")

	def Open(self, waitTime):
		curTime = time.clock()
		self.endTime = curTime + waitTime

		self.Lock()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Unlock()
		self.Hide()

	@ui.WindowDestroy
	def Destroy(self):
		self.Hide()
		self.ClearDictionary()

	def SetText(self, text):
		self.message.SetText(text)

	def SetCountDownMessage(self, waitTime):
		self.countdownMessage.SetText("%.0f%s" % (waitTime, localeInfo.SECOND))

	def SAFE_SetTimeOverEvent(self, event):
		self.eventTimeOver = ui.__mem_func__(event)

	def SAFE_SetExitEvent(self, event):
		self.eventExit = ui.__mem_func__(event)

	def OnUpdate(self):
		lastTime = max(0, self.endTime - time.clock())
		if 0 == lastTime:
			self.Close()
			self.eventTimeOver()
		else:
			self.SetCountDownMessage(self.endTime - time.clock())

	def OnPressExitKey(self):
		#self.eventExit()
		return True

class LoginWindow(ui.ScriptWindow):

	IS_TEST = net.IsTest()

	def __init__(self, stream):
		print "NEW LOGIN WINDOW ----------------------------------------------------------------------------"
		ui.ScriptWindow.__init__(self)
		net.SetPhaseWindow(net.PHASE_WINDOW_LOGIN, self)
		net.SetAccountConnectorHandler(self)

		# localeinfo_point.GetMarcin()

		self.lastLoginTime = 0
		self.inputDialog = None
		self.connectingDialog = None
		self.stream=stream
		self.isNowCountDown=False
		self.isStartError=False

		self.rulesDialog = None
		self.configMain = None

		self.xServerBoard = 0
		self.yServerBoard = 0

		self.loadingImage = None
		self.animatedBackground = False
		self.backgroundFrame = 1
		self.backgroundLastFrameTime = 0.0
		self.backgroundPreloadFrames = []
		self.backgroundPreloadIndex = 0
		self.backgroundPreloadDone = True

		# @fixme001 BEGIN (timeOutMsg and timeOutOk undefined)
		self.timeOutMsg = False
		self.timeOutOk = False
		# @fixme001 END

		self.tooltip = None

		self.save_credential_items = None
		load_credentials()

	def __del__(self):
		net.ClearPhaseWindow(net.PHASE_WINDOW_LOGIN, self)
		net.SetAccountConnectorHandler(0)
		ui.ScriptWindow.__del__(self)
		print "---------------------------------------------------------------------------- DELETE LOGIN WINDOW"

	def Open(self):
		ServerStateChecker.Create(self)

		print "LOGIN WINDOW OPEN ----------------------------------------------------------------------------"

		self.loginFailureMsgDict={
			#"DEFAULT" : localeInfo.LOGIN_FAILURE_UNKNOWN,

			"ALREADY"	: localeInfo.LOGIN_FAILURE_ALREAY,
			"NOID"		: localeInfo.LOGIN_FAILURE_NOT_EXIST_ID,
			"WRONGPWD"	: localeInfo.LOGIN_FAILURE_WRONG_PASSWORD,
			"FULL"		: localeInfo.LOGIN_FAILURE_TOO_MANY_USER,
			"SHUTDOWN"	: localeInfo.LOGIN_FAILURE_SHUTDOWN,
			"REPAIR"	: localeInfo.LOGIN_FAILURE_REPAIR_ID,
			"BLOCK"		: localeInfo.LOGIN_FAILURE_BLOCK_ID,
			"WRONGMAT"	: localeInfo.LOGIN_FAILURE_WRONG_MATRIX_CARD_NUMBER,
			"QUIT"		: localeInfo.LOGIN_FAILURE_WRONG_MATRIX_CARD_NUMBER_TRIPLE,
			"BESAMEKEY"	: localeInfo.LOGIN_FAILURE_BE_SAME_KEY,
			"NOTAVAIL"	: localeInfo.LOGIN_FAILURE_NOT_AVAIL,
			"NOBILL"	: localeInfo.LOGIN_FAILURE_NOBILL,
			"BLKLOGIN"	: localeInfo.LOGIN_FAILURE_BLOCK_LOGIN,
			"WEBBLK"	: localeInfo.LOGIN_FAILURE_WEB_BLOCK,
			"BADSCLID"	: localeInfo.LOGIN_FAILURE_WRONG_SOCIALID,
			"AGELIMIT"	: localeInfo.LOGIN_FAILURE_SHUTDOWN_TIME,

			"NOTACTIV": localeInfo.LOGIN_FAILURE_NOT_ACTIVE,
			"NOPREMIU": localeInfo.LOGIN_FAILURE_NO_PREMIUM,
			"UPDATE": localeInfo.LOGIN_FAILURE_UPDATE,
			"WRONGKEY": localeInfo.LOGIN_FAILURE_ERROR,
			"MAINTENA": localeInfo.LOGIN_FAILURE_MAINTENANCE,
			"DISPUTE": localeInfo.LOGIN_FAILURE_DISPUTE,
			"FAILURE": localeInfo.LOGIN_CONNECT_FAILURE,
		}

		self.loginFailureFuncDict = {
			"WRONGPWD"	: self.__DisconnectAndInputPassword,
			"QUIT"		: app.Exit,
		}

		self.SetSize(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
		self.SetWindowName("LoginWindow")

		if not self.__LoadScript("UIScript/LoginWindow.py"):
			dbg.TraceError("LoginWindow.Open - __LoadScript Error")
			return

		if musicInfo.loginMusic != "":
			snd.SetMusicVolume(systemSetting.GetMusicVolume())
			snd.FadeInMusic("BGM/"+musicInfo.loginMusic)

		snd.SetSoundVolume(systemSetting.GetSoundVolume())

		# pevent key "[" "]"
		ime.AddExceptKey(91)
		ime.AddExceptKey(93)

		self.Show()

		if not app.DEBUG_BUILD and not app.INTERNAL_BUILD:
			if not constInfo.GATEKEEPER_CHECK and app.RestPOSTRequest("logon.mt2009.pl", "80", "/gatekeeper.php"):
				print "success!"

			constInfo.GATEKEEPER_CHECK = True

		# connectingIP = self.stream.GetConnectAddr()
		# if connectingIP:
		# 	self.__OpenLoginBoard()
		# else:
		self.__RefreshServerList()
		self.__OpenServerBoard()

		if not self.rulesDialog:
			dlg = gameRules.GameRulesDialog()
			if gameSettings.GetNumberKey("accept_rules") == gameRules.RULES_VERSION:
				dlg.Hide()
			else:
				dlg.Open()
			self.rulesDialog = dlg

		if not self.configMain:
			dlg = configMain.ConfigWindow()
			dlg.Hide()
			self.configMain = dlg

		app.ShowCursor()

		eventManager.EventManager().add_observer(EVENT_TRY_CONNECT, self.TryConnect)
		eventManager.EventManager().add_observer(EVENT_REQUEST_STATE_CHECK, self.__OnRequestStateCheck)
		self.stream.SetupEvents()
		autologin.OnLoginOpen(self)

	def Close(self):

		if self.connectingDialog:
			self.connectingDialog.Close()
		self.connectingDialog = None

		ServerStateChecker.Initialize(self)

		eventManager.EventManager().clear_event(EVENT_TRY_CONNECT)
		eventManager.EventManager().clear_event(EVENT_REQUEST_STATE_CHECK)

		print "---------------------------------------------------------------------------- CLOSE LOGIN WINDOW "
		#
		#
		if musicInfo.loginMusic != "" and musicInfo.selectMusic != "":
			snd.FadeOutMusic("BGM/"+musicInfo.loginMusic)

		for i in self.save_credential_items:
			i.Destroy()
		self.save_credential_items = None

		self.idEditLine.SetTabEvent(0)
		self.idEditLine.SetReturnEvent(0)
		self.pwdEditLine.SetReturnEvent(0)
		self.pwdEditLine.SetTabEvent(0)

		self.connectBoard = None
		self.loginBoard = None
		if constInfo.ENABLE_SAVE_ACCOUNT:
			self.saveAccountBoard = None
		self.idEditLine = None
		self.pwdEditLine = None
		self.inputDialog = None
		self.connectingDialog = None
		self.loadingImage = None
		self.backgroundPreloadFrames = []

		self.tooltip = None

		if self.rulesDialog:
			self.rulesDialog.Destroy()
			self.rulesDialog = None

		if self.configMain:
			self.configMain.Destroy()
			self.configMain = None

		self.serverBoard				= None
		self.serverList					= None
		self.channelList				= None

		self.KillFocus()
		self.Hide()

		self.stream.popupWindow.Close()
		self.loginFailureFuncDict=None

		ime.ClearExceptKey()

		app.HideCursor()

	def OnIMEReturn(self):
		print "OnIMEReturn"
		self.__OnClickSelectServerButton()
		return True

	def __SaveChannelInfo(self):
		try:
			file=open("channel.inf", "w")
			file.write("%d %d" % (self.__GetServerID(), self.__GetChannelID()))
		except:
			print "LoginWindow.__SaveChannelInfo - SaveError"

	def __LoadChannelInfo(self):
		try:
			file=open("channel.inf")
			lines=file.readlines()

			if len(lines)>0:
				tokens=lines[0].split()

				selServerID=int(tokens[0])
				selChannelID=int(tokens[1])

				return selServerID, selChannelID

		except:
			print "LoginWindow.__LoadChannelInfo - OpenError"
			return -1, -1

	def __ExitGame(self):
		app.Exit()

	def SetIDEditLineFocus(self):
		if self.idEditLine != None:
			self.idEditLine.SetFocus()

	def SetPasswordEditLineFocus(self):
		if constInfo.ENABLE_CLEAN_DATA_IF_FAIL_LOGIN:
			if self.idEditLine != None:
				self.idEditLine.SetText("")
				self.idEditLine.SetFocus()

			if self.pwdEditLine != None:
				self.pwdEditLine.SetText("")
		else:
			if self.pwdEditLine != None:
				self.pwdEditLine.SetFocus()

	def OnEndCountDown(self):
		self.isNowCountDown = False
		self.timeOutMsg = False
		self.OnConnectFailure()

	def OnConnectFailure(self):

		if self.isNowCountDown:
			return

		if autologin.OnConnectFailure(self):
			return

		snd.PlaySound("sound/ui/loginfail.wav")

		if self.connectingDialog:
			self.connectingDialog.Close()
		self.connectingDialog = None

		if self.timeOutMsg:
			self.PopupNotifyMessage(localeInfo.LOGIN_FAILURE_TIMEOUT, self.SetPasswordEditLineFocus)
		else:
			self.PopupNotifyMessage(localeInfo.LOGIN_CONNECT_FAILURE, self.SetPasswordEditLineFocus)

	def OnHandShake(self):
		if not IsLoginDelay():
			snd.PlaySound("sound/ui/loginok.wav")
			self.PopupDisplayMessage(localeInfo.LOGIN_CONNECT_SUCCESS)

	def OnLoginStart(self):
		if not IsLoginDelay():
			self.PopupDisplayMessage(localeInfo.LOGIN_PROCESSING)

	def OnLoginFailure(self, error):
		if autologin.OnLoginFailure(self, error):
			return

		if self.connectingDialog:
			self.connectingDialog.Close()
		self.connectingDialog = None

		try:
			loginFailureMsg = self.loginFailureMsgDict[error]
		except KeyError:
			loginFailureMsg = localeInfo.LOGIN_FAILURE_UNKNOWN + error


		loginFailureFunc=self.loginFailureFuncDict.get(error, self.SetPasswordEditLineFocus)
		self.PopupNotifyMessage(loginFailureMsg, loginFailureFunc)

		snd.PlaySound("sound/ui/loginfail.wav")

	def __DisconnectAndInputID(self):
		if self.connectingDialog:
			self.connectingDialog.Close()
		self.connectingDialog = None

		self.SetIDEditLineFocus()
		net.Disconnect()

	def __DisconnectAndInputPassword(self):
		if self.connectingDialog:
			self.connectingDialog.Close()
		self.connectingDialog = None

		self.SetPasswordEditLineFocus()
		net.Disconnect()

	if constInfo.ENABLE_SAVE_ACCOUNT:
		def SAB_LoadAccountData(self):
			if constInfo.SAB.storeType == constInfo.SAB.ST_CACHE:
				return
			for idx in xrange(constInfo.SAB.slotCount):
				if constInfo.SAB.storeType == constInfo.SAB.ST_REGISTRY:
					id = constInfo.GetWinRegKeyValue(constInfo.SAB.regPath, constInfo.SAB.regName % (idx, constInfo.SAB.regValueId))
					pwd = constInfo.GetWinRegKeyValue(constInfo.SAB.regPath, constInfo.SAB.regName % (idx, constInfo.SAB.regValuePwd))
					if id and pwd:
						self.SAB_SetAccountData(idx, (id, pwd))
				elif constInfo.SAB.storeType == constInfo.SAB.ST_FILE:
					(id, pwd) = constInfo.GetJsonSABData(idx)
					if id and pwd:
						self.SAB_SetAccountData(idx, (id, pwd))

		def SAB_SaveAccountData(self):
			if constInfo.SAB.storeType == constInfo.SAB.ST_CACHE:
				return
			for idx in xrange(constInfo.SAB.slotCount):
				if constInfo.SAB.storeType == constInfo.SAB.ST_REGISTRY:
					_tSlot = self.SAB_GetAccountData(idx)
					if _tSlot:
						(id, pwd) = _tSlot
						constInfo.SetWinRegKeyValue(constInfo.SAB.regPath, constInfo.SAB.regName % (idx, constInfo.SAB.regValueId), id)
						constInfo.SetWinRegKeyValue(constInfo.SAB.regPath, constInfo.SAB.regName % (idx, constInfo.SAB.regValuePwd), pwd)
					else:
						constInfo.DelWinRegKeyValue(constInfo.SAB.regPath, constInfo.SAB.regName % (idx, constInfo.SAB.regValueId))
						constInfo.DelWinRegKeyValue(constInfo.SAB.regPath, constInfo.SAB.regName % (idx, constInfo.SAB.regValuePwd))
				elif constInfo.SAB.storeType == constInfo.SAB.ST_FILE:
					_tSlot = self.SAB_GetAccountData(idx)
					if _tSlot:
						constInfo.SetJsonSABData(idx, _tSlot)
					else:
						constInfo.DelJsonSABData(idx)

		def SAB_DelAccountData(self, slot):
			if constInfo.SAB.accData.get(slot):
				del constInfo.SAB.accData[slot]

		def SAB_GetAccountData(self, slot):
			return constInfo.SAB.accData.get(slot)

		def SAB_SetAccountData(self, slot, data):
			constInfo.SAB.accData[slot] = data

		def SAB_BtnRearrange(self):
			def tooltipArrange(_btnObj):
				_tMexTip = "Account ID: %s" % id
				_btnObj.SetToolTipText(_tMexTip)
				if _btnObj.ToolTipText:
					_btnObj.ToolTipText.SetPackedFontColor(0xff66FFFF)
			## def code
			GetObject=self.GetChild
			SetObject=self.InsertChild
			## button names
			btnNameSave = constInfo.SAB.btnName["Save"]
			btnNameAccess = constInfo.SAB.btnName["Access"]
			btnNameRemove = constInfo.SAB.btnName["Remove"]
			## rearrange code
			for idx in xrange(constInfo.SAB.slotCount):
				_tSlot = self.SAB_GetAccountData(idx)
				# button objects
				btnObjSave = GetObject(btnNameSave % idx)
				btnObjAccess = GetObject(btnNameAccess % idx)
				btnObjRemove = GetObject(btnNameRemove % idx)
				if _tSlot:
					(id, pwd) = _tSlot
					btnObjSave.Hide()
					btnObjAccess.Show()
					btnObjRemove.Show()
					btnObjAccess.SetText(uiScriptLocale.SAVE_ACCOUNT_CONNECT2.format(idx+1, id))
				else:
					btnObjSave.Show()
					btnObjAccess.Hide()
					btnObjRemove.Hide()
			# done

		def SAB_Click_Save(self, slot):
			if slot >= constInfo.SAB.slotCount:
				return
			## def code
			GetObject=self.GetChild
			SetObject=self.InsertChild
			## button stuff
			_tmpName = constInfo.SAB.btnName["Save"] % slot
			_tmpObj = GetObject(_tmpName)
			## code stuff
			try:
				id = self.idEditLine.GetText()
				pwd = self.pwdEditLine.GetText()

				if len(id)==0:
					self.PopupNotifyMessage(localeInfo.LOGIN_INPUT_ID, self.SetIDEditLineFocus)
					return

				if len(pwd)==0:
					self.PopupNotifyMessage(localeInfo.LOGIN_INPUT_PASSWORD, self.SetPasswordEditLineFocus)
					return
			except:
				return
			self.SAB_SetAccountData(slot, (id,pwd))
			self.SAB_SaveAccountData()
			## rearrange stuff
			self.SAB_BtnRearrange()

		def SAB_Click_Access(self, slot):
			if slot >= constInfo.SAB.slotCount:
				return
			## def code
			GetObject=self.GetChild
			SetObject=self.InsertChild
			## button stuff
			_tmpName = constInfo.SAB.btnName["Access"] % slot
			_tmpObj = GetObject(_tmpName)
			## code stuff
			_tSlot = self.SAB_GetAccountData(slot)
			if _tSlot:
				(id, pwd) = _tSlot
				self.idEditLine.SetText(id)
				self.pwdEditLine.SetText(pwd)
				self.__OnClickSelectServerButton()
				self.__OnClickLoginButton()

		def SAB_Click_Remove(self, slot):
			if slot >= constInfo.SAB.slotCount:
				return
			## def code
			GetObject=self.GetChild
			SetObject=self.InsertChild
			## button stuff
			_tmpName = constInfo.SAB.btnName["Remove"] % slot
			_tmpObj = GetObject(_tmpName)
			## code stuff
			self.SAB_DelAccountData(slot)
			self.SAB_SaveAccountData()
			## rearrange stuff
			self.SAB_BtnRearrange()

		def __CreateSaveAccountBoard(self):
			### SAB INIT
			self.SAB_LoadAccountData()
			## def code
			GetObject=self.GetChild
			SetObject=self.InsertChild
			## gui stuff
			SCREEN_WIDTH = wndMgr.GetScreenWidth()
			SCREEN_HEIGHT = wndMgr.GetScreenHeight()
			## button space
			SPACE_FOR_BUTTON = 25+1
			ALL_BUTTON_SPACE = SPACE_FOR_BUTTON * constInfo.SAB.slotCount
			## board stuff
			BOARD_SIZE = (210+120, 28 + ALL_BUTTON_SPACE)
			BOARD_POS = ((SCREEN_WIDTH - 208) / 2 + 210, (SCREEN_HEIGHT - 410) - (10*constInfo.SAB.slotCount))
			## button stuff
			btnNameSave = constInfo.SAB.btnName["Save"]
			btnNameAccess = constInfo.SAB.btnName["Access"]
			btnNameRemove = constInfo.SAB.btnName["Remove"]
			btnPath = "d:/ymir work/ui/public/%s_button_%02d.sub" # xsmall small middle large xlarge big
			btnImage = {"default":1,"over":2,"down":3}
			## SAB BOARD
			try:
				## default init
				_tmpName = "SaveAccountBoard"
				SetObject(_tmpName, ui.ThinBoard())
				#
				_tmpObj = GetObject(_tmpName)
				_tmpObj.SetParent(self)
				## custom data
				_tmpObj.SetSize(*BOARD_SIZE)
				## default data
				_tmpObj.SetPosition(*BOARD_POS)
				_tmpObj.Show()
				self.saveAccountBoard = _tmpObj
			except:
				import exception; exception.Abort("__CreateSaveAccountBoard SAB BOARD")
			### SAB TITLE
			try:
				## default init
				_tmpName = "SaveAccountTitle"
				SetObject(_tmpName, ui.TextLine())
				_tmpObj = GetObject(_tmpName)
				_tmpObj.SetParent(self.saveAccountBoard)
				## custom data
				_tmpObj.SetHorizontalAlignCenter()
				_tmpObj.SetPackedFontColor(0xFFffbf00)
				_tmpObj.SetOutline()
				_tmpObj.SetText(uiScriptLocale.SAVE_ACCOUNT_TITLE)
				## default data
				_tmpObj.SetPosition(BOARD_SIZE[0]/2, 5)
				_tmpObj.Show()
			except:
				import exception; exception.Abort("__CreateSaveAccountBoard SAB TITLE")
			### SAB LINE
			try:
				## default init
				_tmpName = "SaveAccountLine"
				SetObject(_tmpName, ui.Line())
				_tmpObj = GetObject(_tmpName)
				_tmpObj.SetParent(self.saveAccountBoard)
				## custom data
				_tmpObj.SetColor(0xFF777777)
				_tmpObj.SetSize(BOARD_SIZE[0]-10, 0)
				## default data
				_tmpObj.SetPosition(5, 20)
				_tmpObj.Show()
			except:
				import exception; exception.Abort("__CreateSaveAccountBoard SAB LINE")
			## SaveAccountButtons
			for idx in xrange(constInfo.SAB.slotCount):
				### SAB SAVE
				try:
					## default init
					_tmpName = btnNameSave % (idx)
					SetObject(_tmpName, ui.Button())
					_tmpObj = GetObject(_tmpName)
					_tmpObj.SetParent(self.saveAccountBoard)
					## custom data
					_tmpBtnPath = "d:/ymir work/ui/public/xlarge_button_%02d.sub" # xsmall small middle large xlarge big
					_tmpObj.SetUpVisual(_tmpBtnPath % (btnImage["default"]))
					_tmpObj.SetOverVisual(_tmpBtnPath % (btnImage["over"]))
					_tmpObj.SetDownVisual(_tmpBtnPath % (btnImage["down"]))
					_tmpObj.SetText(uiScriptLocale.SAVE_ACCOUNT_SAVE)
					_tmpObj.SAFE_SetEvent(self.SAB_Click_Save, idx)
					## default data
					_tmpObj.SetPosition(15 + 60, 25 + (idx * SPACE_FOR_BUTTON))
					_tmpObj.Hide()
				except:
					import exception; exception.Abort("__CreateSaveAccountBoard SAB SAVE")
				### SAB ACCESS
				try:
					## default init
					_tmpName = btnNameAccess % (idx)
					SetObject(_tmpName, ui.Button())
					_tmpObj = GetObject(_tmpName)
					_tmpObj.SetParent(self.saveAccountBoard)
					## custom data
					_tmpBtnPath = "d:/ymir work/ui/public/xlarge_button_%02d.sub" # xsmall small middle large xlarge big
					_tmpObj.SetUpVisual(_tmpBtnPath % (btnImage["default"]))
					_tmpObj.SetOverVisual(_tmpBtnPath % (btnImage["over"]))
					_tmpObj.SetDownVisual(_tmpBtnPath % (btnImage["down"]))
					_tmpObj.SetText(uiScriptLocale.SAVE_ACCOUNT_CONNECT.format(idx+1))
					_tmpObj.SAFE_SetEvent(self.SAB_Click_Access, idx)
					## default data
					_tmpObj.SetPosition(35, 25 + (idx * SPACE_FOR_BUTTON))
					_tmpObj.Show()
				except:
					import exception; exception.Abort("__CreateSaveAccountBoard SAB ACCESS")
				### SAB REMOVE
				try:
					## default init
					_tmpName = btnNameRemove % (idx)
					SetObject(_tmpName, ui.Button())
					_tmpObj = GetObject(_tmpName)
					_tmpObj.SetParent(self.saveAccountBoard)
					## custom data
					_tmpBtnPath = "d:/ymir work/ui/public/middle_button_%02d.sub" # xsmall small middle large xlarge big
					_tmpObj.SetUpVisual(_tmpBtnPath % (btnImage["default"]))
					_tmpObj.SetOverVisual(_tmpBtnPath % (btnImage["over"]))
					_tmpObj.SetDownVisual(_tmpBtnPath % (btnImage["down"]))
					_tmpObj.SetText(uiScriptLocale.SAVE_ACCOUNT_REMOVE)
					_tmpObj.SAFE_SetEvent(self.SAB_Click_Remove, idx)
					## default data
					_tmpObj.SetPosition(35 + 190, 25 + (idx * SPACE_FOR_BUTTON))
					_tmpObj.Show()
				except:
					import exception; exception.Abort("__CreateSaveAccountBoard SAB REMOVE")
			self.SAB_BtnRearrange()

	def __PreloadAnimatedBackground(self):
		# Only create the hidden ImageBox holders here. The actual texture
		# uploads are streamed in gradually from OnUpdate (see
		# __UpdateAnimatedBackgroundPreload) so opening the window doesn't
		# block on 32 synchronous full-HD texture loads.
		self.backgroundPreloadFrames = []
		for frameNumber in xrange(1, 33):
			frameImage = ui.ImageBox()
			frameImage.SetParent(self.background)
			frameImage.Hide()
			self.backgroundPreloadFrames.append(frameImage)
		self.backgroundPreloadIndex = 0
		self.backgroundPreloadDone = False

	def __UpdateAnimatedBackgroundPreload(self):
		if self.backgroundPreloadDone:
			return
		framesPerTick = 2
		for i in xrange(framesPerTick):
			if self.backgroundPreloadIndex >= len(self.backgroundPreloadFrames):
				self.backgroundPreloadDone = True
				break
			frameNumber = self.backgroundPreloadIndex + 1
			self.backgroundPreloadFrames[self.backgroundPreloadIndex].LoadImage("locale/pl/ui/animated/login_big_%02d.dds" % frameNumber)
			self.backgroundPreloadIndex += 1

	def __LoadScript(self, fileName):
		import dbg
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, fileName)
		except:
			import exception
			exception.Abort("LoginWindow.__LoadScript.LoadObject")
		try:
			GetObject=self.GetChild
			self.serverBoard			= GetObject("ServerBoard")
			self.serverList				= GetObject("ServerList")
			self.channelList			= GetObject("ChannelList")
			self.serverSelectButton		= GetObject("ServerSelectButton")
			self.serverExitButton		= GetObject("ServerExitButton")
			self.connectBoard			= GetObject("ConnectBoard")
			self.loginBoard				= GetObject("LoginBoard")
			self.idEditLine				= GetObject("ID_EditLine")
			self.pwdEditLine			= GetObject("Password_EditLine")
			self.serverInfo				= GetObject("ConnectName")
			self.selectConnectButton	= GetObject("SelectConnectButton")
			self.loginButton			= GetObject("LoginButton")
			self.loginExitButton		= GetObject("LoginExitButton")

			self.background = GetObject("background")
			self.rulesButton = GetObject("RulesButton")
			self.settingsButton = GetObject("SettingsButton")
			self.versionLabel = GetObject("Version")
			self.credentialsWindow = GetObject("Credentials")
			self.homePageButton = GetObject("HomePageButton")
			self.facebookButton = GetObject("FacebookButton")
			self.discordButton = GetObject("DiscordButton")

			if constInfo.ENABLE_SAVE_ACCOUNT:
				self.__CreateSaveAccountBoard()

			screen_width = wndMgr.GetScreenWidth()
			screen_height = wndMgr.GetScreenHeight()
			if screen_width < 1000:
				self.background.LoadImage("locale/pl/ui/login_small.jpg")
				self.background.SetScale(float(screen_width) / 800.0, float(screen_height) / 600.0)
			elif screen_width > 1280:
				self.background.LoadImage("locale/pl/ui/animated/login_big_01.jpg")
				self.background.SetScale(float(screen_width) / 1920.0, float(screen_height) / 1080.0)
				self.animatedBackground = True
				self.backgroundFrame = 1
				self.backgroundLastFrameTime = time.clock()
				self.__PreloadAnimatedBackground()

			self.rulesButton.SAFE_SetEvent(self.__OnClickRulesButton)
			self.settingsButton.SAFE_SetEvent(self.__OnClickSettingsButton)

		except:
			import exception
			exception.Abort("LoginWindow.__LoadScript.BindObject")

		if self.IS_TEST:
			self.selectConnectButton.Hide()
		else:
			self.selectConnectButton.SetEvent(ui.__mem_func__(self.__OnClickSelectConnectButton))

		self.tooltip = uiToolTip.ToolTip()

		self.serverBoard.OnKeyUp = ui.__mem_func__(self.__ServerBoard_OnKeyUp)
		self.xServerBoard, self.yServerBoard = self.serverBoard.GetLocalPosition()

		self.serverSelectButton.SetEvent(ui.__mem_func__(self.__OnClickSelectServerButton))
		self.serverExitButton.SetEvent(ui.__mem_func__(self.__OnClickExitButton))

		self.loginButton.SetEvent(ui.__mem_func__(self.__OnClickLoginButton))
		self.loginExitButton.SetEvent(ui.__mem_func__(self.__OnClickExitButton))

		self.channelList.OnKeyUp = ui.__mem_func__(self.__ServerBoard_OnKeyUp)
		self.serverList.SetEvent(ui.__mem_func__(self.__OnSelectServer))

		self.idEditLine.SetReturnEvent(ui.__mem_func__(self.pwdEditLine.SetFocus))
		self.idEditLine.SetTabEvent(ui.__mem_func__(self.pwdEditLine.SetFocus))

		self.pwdEditLine.SetReturnEvent(ui.__mem_func__(self.__OnClickLoginButton))
		self.pwdEditLine.SetTabEvent(ui.__mem_func__(self.idEditLine.SetFocus))

		self.homePageButton.SAFE_SetEvent(self.OpenURL, "https://github.com/TieruYT/metin2-playerbots")
		self.facebookButton.SAFE_SetEvent(self.OpenURL, "https://buycoffee.to/metin2-playerbots")
		self.discordButton.SAFE_SetEvent(self.OpenURL, "https://discord.gg/pt5tvnrN6")

		self.save_credential_items = []
		for i in range(MAX_CREDENTIALS_SAVE):
			item = SaveCredentialsItem(i, self, self.credentialsWindow)
			item.SetToolTip(self.tooltip)
			item.Show()
			self.save_credential_items.append(item)

		self.SetVersion()
		return 1

	def OpenURL(self, url):
		utils.open_url(url)

	def SetVersion(self):
		data = constInfo.GAME_VERSION
		version_string = "Wersja: %d.%d.%d%s" % (
		data["expansion"], data["major"], data["minor"], "b" if data["is_beta"] else "")
		self.versionLabel.SetText(version_string)

	def Connect(self, id, pwd):

		if constInfo.SEQUENCE_PACKET_ENABLE:
			net.SetPacketSequenceMode()

		if IsLoginDelay():
			loginDelay = GetLoginDelay()
			self.connectingDialog = ConnectingDialog()
			self.connectingDialog.Open(loginDelay)
			self.connectingDialog.SAFE_SetTimeOverEvent(self.OnEndCountDown)
			self.connectingDialog.SAFE_SetExitEvent(self.OnPressExitKey)
			self.isNowCountDown = True
		else:
			self.stream.popupWindow.Close()
			self.stream.popupWindow.Open(localeInfo.LOGIN_CONNETING, self.SetPasswordEditLineFocus, localeInfo.UI_CANCEL)

		self.stream.SetLoginInfo(id, pwd)
		eventManager.EventManager().send_delayed_event(EVENT_TRY_CONNECT, 0.2)

	def TryConnect(self):
		self.stream.Connect()

	def __OnClickRulesButton(self):
		self.rulesDialog.Open()

	def __OnClickSettingsButton(self):
		self.configMain.Open()

	def __OnClickExitButton(self):
		self.stream.SetPhaseWindow(0)

	def __SetServerInfo(self, name):
		net.SetServerInfo(name.strip())
		self.serverInfo.SetText(name)

	def PopupDisplayMessage(self, msg):
		self.stream.popupWindow.Close()
		self.stream.popupWindow.Open(msg)

	def PopupNotifyMessage(self, msg, func=0):
		if not func:
			func=self.EmptyFunc

		self.stream.popupWindow.Close()
		self.stream.popupWindow.Open(msg, func, localeInfo.UI_OK)

	def __OnCloseInputDialog(self):
		if self.inputDialog:
			self.inputDialog.Close()
		self.inputDialog = None
		return True

	def OnPressExitKey(self):
		self.stream.popupWindow.Close()
		self.stream.SetPhaseWindow(0)
		return True

	def OnExit(self):
		self.stream.popupWindow.Close()
		self.stream.popupWindow.Open(localeInfo.LOGIN_FAILURE_WRONG_MATRIX_CARD_NUMBER_TRIPLE, app.Exit, localeInfo.UI_OK)

	def OnUpdate(self):
		eventManager.EventManager().Update()
		ServerStateChecker.Update()
		autologin.PumpLogin(self)

		if self.animatedBackground and not self.backgroundPreloadDone:
			self.__UpdateAnimatedBackgroundPreload()

		# 32 frames, 12 FPS; enabled only for login_big (screen width > 1280 px).
		if self.animatedBackground and time.clock() - self.backgroundLastFrameTime >= 0.083:
			self.backgroundFrame = (self.backgroundFrame % 32) + 1
			self.background.LoadImage("locale/pl/ui/animated/login_big_%02d.dds" % self.backgroundFrame)
			self.backgroundLastFrameTime = time.clock()

	def EmptyFunc(self):
		pass

	#####################################################################################

	def __ServerBoard_OnKeyUp(self, key):
		if self.serverBoard.IsShow():
			if app.DIK_RETURN==key:
				self.__OnClickSelectServerButton()
		return True

	def __GetServerID(self):
		return self.serverList.GetSelectedItem()

	def __GetChannelID(self):
		return self.channelList.GetSelectedItem()

	def __OpenServerBoard(self):
		serverIndex = self.__GetServerID()
		channelIndex = self.__GetChannelID()
		self.serverList.SelectItem(serverIndex)

		if channelIndex >= 0:
			self.channelList.SelectItem(channelIndex)
		else:
			self.channelList.SelectItem(app.GetRandom(0, self.channelList.GetItemCount() - 1))

		self.serverBoard.SetPosition(self.xServerBoard, self.yServerBoard)
		self.serverBoard.Show()
		self.connectBoard.Hide()
		self.loginBoard.Hide()
		if constInfo.ENABLE_SAVE_ACCOUNT:
			self.saveAccountBoard.Hide()
		self.credentialsWindow.Hide()

		self.KillInputFocus() #@fixme019

	def KillInputFocus(self): #@fixme019
		if self.idEditLine and self.idEditLine.IsFocus():
			self.idEditLine.KillFocus()
		if self.pwdEditLine and self.pwdEditLine.IsFocus():
			self.pwdEditLine.KillFocus()

	def __OpenLoginBoard(self):

		self.serverExitButton.SetEvent(ui.__mem_func__(self.__OnClickExitServerButton))
		self.serverExitButton.SetText(localeInfo.UI_CLOSE)

		self.serverBoard.SetPosition(self.xServerBoard, wndMgr.GetScreenHeight())
		self.serverBoard.Hide()

		self.connectBoard.Show()
		self.loginBoard.Show()
		if constInfo.ENABLE_SAVE_ACCOUNT:
			self.saveAccountBoard.Show()

		self.credentialsWindow.Show()

		## if users have the login infomation, then don't initialize.2005.9 haho
		if self.idEditLine == None:
			self.idEditLine.SetText("")
		if self.pwdEditLine == None:
			self.pwdEditLine.SetText("")

		self.idEditLine.SetFocus()

	def __RefreshServerList(self):
		self.serverList.ClearItem()

		# SEVER_LIST_BUG_FIX
		visible_index = 1
		for id, server_data in serverInfo.SERVER_LIST.items():
			name = server_data["main"].get("name", "noname")
			self.serverList.InsertItem(id, "  %02d. %s" % (int(visible_index), name))
			visible_index += 1
		# END_OF_SEVER_LIST_BUG_FIX

	def __OnSelectServer(self):
		self.__OnCloseInputDialog()
		self.__RequestServerStateList()
		self.__RefreshServerStateList()

	def __RequestServerStateList(self):
		serverID = self.__GetServerID()
		try:
			channelDict = serverInfo.SERVER_LIST[serverID]["channel"]
		except:
			print " __RequestServerStateList - serverInfo.SERVER_LIST(%d)" % (serverID)
			return

		ServerStateChecker.Initialize()

		firstChannelData = {}
		for id, channelDataDict in channelDict.items():
			key=serverID*100 + id
			ip=channelDataDict["ip"]
			udp_port=channelDataDict["udp_port"]
			ServerStateChecker.AddChannel(key, ip, udp_port)

			if not firstChannelData.has_key("host"):
				firstChannelData["host"] = ip
				firstChannelData["port"] = udp_port

		if not app.DEBUG_BUILD:
			# net.SendPingPort(firstChannelData["host"], firstChannelData["port"])
			eventManager.EventManager().send_delayed_event(EVENT_REQUEST_STATE_CHECK, 2)
		else:
			self.__OnRequestStateCheck()

	def __OnRequestStateCheck(self):
		ServerStateChecker.Request()

	def __RefreshServerStateList(self):
		serverID = self.__GetServerID()
		bakChannelID = self.channelList.GetSelectedItem()

		self.channelList.ClearItem()

		try:
			channelDict = serverInfo.SERVER_LIST[serverID]["channel"]
		except:
			print " __RequestServerStateList - serverInfo.SERVER_LIST(%d)" % (serverID)
			return

		shown = []
		for channelID, channelDataDict in channelDict.items():
			channelName = channelDataDict["name"]
			channelState = channelDataDict["state"]
			# The second channel runs only when the server switches it on
			# (M2_PLAYERBOT_CH2): listed once it answers, never as a dead line.
			# CH1 is always the first line, so a line is still its channel.
			if channelID > 0 and channelState in (serverInfo.STATE_NONE, serverInfo.STATE_DICT[0]):
				continue
			self.channelList.InsertItem(channelID, "%s %s" % (channelName, channelState))
			shown.append(channelID)

		if bakChannelID not in shown:
			bakChannelID = 0
		self.channelList.SelectItem(bakChannelID)

	def NotifyChannelState(self, addrKey, state):
		serverID = self.__GetServerID()
		channelID = addrKey % 100

		try:
			stateName=serverInfo.STATE_DICT[state]
		except:
			stateName=serverInfo.STATE_NONE

		try:
			serverInfo.SERVER_LIST[serverID]["channel"][channelID]["state"] = stateName
			self.__RefreshServerStateList()

		except:
			import exception
			exception.Abort(localeInfo.CHANNEL_NOT_FIND_INFO)

	def __OnClickExitServerButton(self):
		print "exit server"
		self.__OpenLoginBoard()

	def __OnClickSelectServerButton(self):
		serverID = self.__GetServerID()
		channelID = self.__GetChannelID()

		if (not serverInfo.SERVER_LIST.has_key(serverID)):
			self.PopupNotifyMessage(localeInfo.CHANNEL_SELECT_SERVER)
			return

		serverData = serverInfo.SERVER_LIST[serverID]

		try:
			channelDict = serverData["channel"][channelID]
		except KeyError:
			return

		try:
			state = channelDict["state"]
		except KeyError:
			self.PopupNotifyMessage(localeInfo.CHANNEL_SELECT_CHANNEL)
			return

		if state == serverInfo.STATE_DICT[3]:
			self.PopupNotifyMessage(localeInfo.CHANNEL_NOTIFY_FULL)
			return

		self.__SaveChannelInfo()

		try:
			serverName = serverData["main"]["name"]
			channelName = channelDict["name"]

		except:
			print " ERROR __OnClickSelectServerButton(%d, %d)" % (serverID, channelID)
			serverName = localeInfo.CHANNEL_EMPTY_SERVER
			channelName = localeInfo.CHANNEL_NORMAL % channelID

		self.__SetServerInfo("%s, %s " % (serverName, channelName))

		try:
			ip = channelDict["ip"]
			tcp_port = channelDict["tcp_port"]
		except:
			import exception
			exception.Abort("LoginWindow.__OnClickSelectServerButton")

		try:
			account_ip = serverData["auth"][channelID]["ip"]

			port_list = serverData["auth"][channelID]["port"]
			random_channel_port_index = app.GetRandom(0, len(port_list)-1)
			account_port = port_list[random_channel_port_index]
		except:
			account_ip = 0
			account_port = 0

		try:
			markDict=serverData["mark"]
			markIP = serverData["main"]["host"]
			markPort = serverData["main"]["mark"]
			net.SetMarkServer(markIP, markPort)
			app.SetGuildMarkPath(markDict["mark"])
			# GUILD_SYMBOL
			app.SetGuildSymbolPath(markDict["symbol_path"])
			# END_OF_GUILD_SYMBOL

		except:
			import exception
			exception.Abort("LoginWindow.__OnClickSelectServerButton")

		self.stream.SetConnectInfo(ip, tcp_port, account_ip, account_port)
		self.__OpenLoginBoard()

	def __OnClickSelectConnectButton(self):
		self.__RefreshServerList()
		self.__OpenServerBoard()

	def OnClickLoginButton(self):
		self.__OnClickLoginButton()

	def __OnClickLoginButton(self):
		id = self.idEditLine.GetText()
		pwd = self.pwdEditLine.GetText()

		if len(id)==0:
			self.PopupNotifyMessage(localeInfo.LOGIN_INPUT_ID, self.SetIDEditLineFocus)
			return

		if len(pwd)==0:
			self.PopupNotifyMessage(localeInfo.LOGIN_INPUT_PASSWORD, self.SetPasswordEditLineFocus)
			return

		self.Connect(id, pwd)

	def OnKeyDown(self, key):
		# if constInfo.ENABLE_SAVE_ACCOUNT:
		# 	for idx in xrange(constInfo.SAB.slotCount):
		# 		if app.DIK_F1+idx == key and self.SAB_GetAccountData(idx):
		# 			self.SAB_Click_Access(idx)

		for idx in xrange(MAX_CREDENTIALS_SAVE):
			if app.DIK_F1+idx == key and self.save_credential_items[idx].IsSaved():
				self.__OnClickSelectServerButton()
				self.save_credential_items[idx].Load()
		return True
