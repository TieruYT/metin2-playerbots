import ui
import app
import wndMgr
import systemSetting
import uiCommon
import localeInfo
import uiScriptLocale
import mathUtils
import math
import colorInfo
import snd
import net

class ConfigWindow(ui.ScriptWindow):
	class ResolutionConfig:
		def __init__(self, config):
			self.config = config
			self.dest_width = 0
			self.dest_height = 0
			self.dest_bpp = 0
			self.changed = 0
			self.Load()

		def Destroy(self):
			self.config = None
			self.resolutionList = None
			self.resolutionScrollBar = None

		def Load(self):
			screen_width = systemSetting.GetWidth()
			screen_height = systemSetting.GetHeight()

			self.resolutionList = self.config.GetChild("Resolution")
			found_resolution = False
			for i in range(systemSetting.RESOLUTION_MAX_NUM):
				width, height, bpp = systemSetting.GetResolution(i)
				if width == 0 or height == 0:
					break

				res_name = "{0}x{1} {2}bpp".format(width, height, bpp)
				self.resolutionList.InsertItem(i, res_name)

				if screen_width == width and screen_height == height:
					self.resolutionList.SetCurrentItem(res_name)
					self.resolutionList.SelectItem(i)
					found_resolution = True

			if not found_resolution:
				self.resolutionList.SetCurrentItem("Niestandardowa")

			self.resolutionList.SAFE_SetEvent(self.OnResolutionSelect)

		def IsChanged(self):
			return self.changed

		def OnResolutionSelect(self, index):
			self.changed = 1
			width, height, bpp = systemSetting.GetResolution(index)
			res_name = "{0}x{1} {2}bpp".format(width, height, bpp)
			self.resolutionList.SetCurrentItem(res_name)
			self.dest_width = width
			self.dest_height = height
			self.dest_bpp = bpp
			self.config.scale_interface_config.RefreshScaleLabel()

	class CustomResolutionConfig:
		def __init__(self, config):
			self.config = config
			self.Load()

		def Destroy(self):
			self.config = None
			self.widthEditLine = None
			self.heightEditLine = None

		def Load(self):
			self.widthEditLine = self.config.GetChild("WidthEditLine")
			self.heightEditLine = self.config.GetChild("HeightEditLine")

			screen_width = systemSetting.GetWidth()
			screen_height = systemSetting.GetHeight()

			self.widthEditLine.SetText(str(screen_width))
			self.heightEditLine.SetText(str(screen_height))
			self.config.scale_interface_config.RefreshScaleLabel()

		def IsChanged(self):
			screen_width, screen_height = systemSetting.GetCustomResolution()
			return int(self.widthEditLine.GetText()) != screen_width or int(self.heightEditLine.GetText()) != screen_height

		def GetResolution(self):
			return int(self.widthEditLine.GetText()), int(self.heightEditLine.GetText())

	class ScreenModeConfig:
		def __init__(self, config):
			self.config = config
			self.Load()

		def Destroy(self):
			self.config = None
			self.windowModeButton = None
			self.fullScreenModeButton = None

		def Load(self):
			self.windowModeButton = self.config.GetChild("WindowMode")
			self.fullScreenModeButton = self.config.GetChild("FullscreenMode")
			self.fullWindowModeButton = self.config.GetChild("FullscreenWindowMode")
			self.button_group = [self.fullScreenModeButton, self.windowModeButton, self.fullWindowModeButton]

			self.fullScreenModeButton.SAFE_SetEvent(self.OnButtonClick, 0)
			self.windowModeButton.SAFE_SetEvent(self.OnButtonClick, 1)
			self.fullWindowModeButton.SAFE_SetEvent(self.OnButtonClick, 2)

			self.is_windowed = systemSetting.IsWindowed()
			self.OnButtonClick(self.is_windowed)

		def IsChanged(self):
			return systemSetting.IsWindowed() != self.is_windowed

		def OnButtonClick(self, value):
			for i in self.button_group:
				i.Enable()
				i.SetUp()

			self.button_group[value].Down()
			self.button_group[value].Disable()

			self.is_windowed = value

	class ScaleInterfaceConfig:
		SCALE_MODES = [uiScriptLocale.CONFIG_SCALE_MODE_NONE, uiScriptLocale.CONFIG_SCALE_MODE_YES]
		def __init__(self, config):
			self.config = config
			self.is_scale_mode = systemSetting.IsScaleMode()
			self.popup = None
			self.Load()

		def Destroy(self):
			self.config = None
			self.scaleModeList = None
			self.scaleSlider = None
			self.scaleLabel = None
			self.popup = None

		def Load(self):
			self.scaleModeList = self.config.GetChild("ScaleModeSelectBar")
			self.scaleSlider = self.config.GetChild("ScaleSlider")
			self.scaleLabel = self.config.GetChild("ScaleLabel")

			self.scaleSlider.SetEvent(ui.__mem_func__(self.__OnScaleSlider))
			self.scaleSlider.SetSliderPos(self.__GetSliderPos(systemSetting.GetScaleInterface()))
			self.__OnScaleSlider()

			for i in range(len(self.SCALE_MODES)):
				self.scaleModeList.InsertItem(i, self.SCALE_MODES[i])
			self.scaleModeList.SAFE_SetEvent(self.OnScaleModeSelect)
			self.scaleModeList.SelectItem(self.GetScaleMode())

		def __OpenPopup(self):
			if not self.IsChanged():
				return

			if self.popup:
				self.popup.Destroy()
				self.popup = None

			dlg = uiCommon.PopupDialog()
			dlg.SetText(localeInfo.CONFIG_SCALE_MODE_WARNING)
			dlg.SetWidth(380)
			dlg.Open()
			self.popup = dlg


		def __GetSliderPos(self, scale):
			start = 1.0
			end = 3.0
			delta = scale - start
			return mathUtils.Clamp01(delta / (end - start))

		def __OnScaleSlider(self):
			pos = self.scaleSlider.GetSliderPos()
			# self.scale = mathUtils.GetLerp(1.0, 3.0, pos)
			self.scale = min(1 + int(pos / 0.33), 3)
			scale_string = "%.1f" % self.scale
			self.scaleLabel.SetText("Skala (%s)" % scale_string)
			self.RefreshScaleLabel()
			systemSetting.SetScaleInterface(self.scale)

		def RefreshScaleLabel(self):
			scale_string = "%.1f" % self.scale
			current_width, current_height = self.config.GetResolution()
			if current_height / float(scale_string) < 600:  # minimum height
				self.scaleLabel.SetPackedFontColor(colorInfo.NEGATIVE_COLOR)
			elif scale_string == "2.0":
				self.scaleLabel.SetPackedFontColor(colorInfo.POSITIVE_COLOR)
			else:
				self.scaleLabel.SetPackedFontColor(colorInfo.FONT_COLOR)

		def GetScaleMode(self):
			if systemSetting.IsScaleMode():
				return 1
			else:
				return 0

		def IsChanged(self):
			if systemSetting.IsScaleMode() != self.is_scale_mode:
				return True

			current_scale = "%.1f" % systemSetting.GetScaleInterface()
			new_scale = "%.1f" % self.scale
			return current_scale != new_scale

		def OnScaleModeSelect(self, index):
			self.scaleModeList.SetCurrentItem(self.SCALE_MODES[index])
			self.is_scale_mode = index == 1
			if self.is_scale_mode:
				self.__OpenPopup()

	class FontConfig:
		AVAILABLE_FONTS = {
			0 : {
				"name": localeInfo.CONFIG_FONT_OPTION1,
				"font" : "Tahoma:12",
			},
			1: {
				"name": localeInfo.CONFIG_FONT_OPTION2,
				"font": "Tahoma:14",
			},
			2: {
				"name": localeInfo.CONFIG_FONT_OPTION3,
				"font": "Arial:14",
			}
		}
		def __init__(self, config):
			self.config = config
			self.dest_font = systemSetting.GetFontName()
			self.dest_size = systemSetting.GetFontSize()
			self.Load()

		def Destroy(self):
			self.config = None
			self.fontList = None

		def GetCurrentFontIndex(self):
			current = systemSetting.GetFont()
			for key,val in self.AVAILABLE_FONTS.items():
				if val["font"] == current:
					return key
			return 0

		def Load(self):
			self.fontList = self.config.GetChild("FontSelect")
			for i in range(len(self.AVAILABLE_FONTS)):
				font_data = self.AVAILABLE_FONTS[i]
				self.fontList.InsertItem(i, font_data["name"], None, font_data["font"])

			self.fontList.SAFE_SetEvent(self.OnFontSelect)
			self.fontList.SelectItem(self.GetCurrentFontIndex())

		def IsChanged(self):
			return self.dest_font != systemSetting.GetFontName() or self.dest_size != systemSetting.GetFontSize()

		def OnFontSelect(self, index):
			font_data = self.AVAILABLE_FONTS[index]
			self.fontList.SetCurrentItem(font_data["name"])
			font_split = font_data["font"].split(":")
			self.dest_font = font_split[0]
			self.dest_size = int(font_split[1])

	class MultiSamplingConfig:
		SAMPLING_OPTIONS = [uiScriptLocale.CONFIG_MUTLISAMPLING_LEVEL0, uiScriptLocale.CONFIG_MUTLISAMPLING_LEVEL1, uiScriptLocale.CONFIG_MUTLISAMPLING_LEVEL2, uiScriptLocale.CONFIG_MUTLISAMPLING_LEVEL3]

		def __init__(self, config):
			self.config = config
			self.dest_sampling = systemSetting.GetSamplingLevel()
			self.Load()

		def Destroy(self):
			self.config = None
			self.samplingList = None

		def Load(self):
			self.samplingList = self.config.GetChild("SamplingSelectBar")
			for i in range(len(self.SAMPLING_OPTIONS)):
				self.samplingList.InsertItem(i, self.SAMPLING_OPTIONS[i])

			self.samplingList.SAFE_SetEvent(self.OnSamplingSelect)
			self.samplingList.SelectItem(systemSetting.GetSamplingLevel())

		def IsChanged(self):
			return self.dest_sampling != systemSetting.GetSamplingLevel()

		def OnSamplingSelect(self, index):
			self.samplingList.SetCurrentItem(self.SAMPLING_OPTIONS[index])
			self.dest_sampling = index

	class ShadowConfig:
		SHADOW_OPTIONS = [uiScriptLocale.CONFIG_SHADOW_NONE,
							uiScriptLocale.CONFIG_SHADOW_GROUND,
							uiScriptLocale.CONFIG_SHADOW_GROUND_AND_SOLO,
							uiScriptLocale.CONFIG_SHADOW_GROUND_ALL,
							uiScriptLocale.CONFIG_SHADOW_GROUND_ALL_HIGH,
							uiScriptLocale.CONFIG_SHADOW_GROUND_ALL_MAX,
							uiScriptLocale.CONFIG_SHADOW_GROUND_ALL_MAX_4K,
							uiScriptLocale.CONFIG_SHADOW_GROUND_ALL_MAX_8K]

		def __init__(self, config):
			self.config = config
			self.dest_shadow = systemSetting.GetShadowLevel()
			self.Load()

		def Destroy(self):
			self.config = None
			self.shadowList = None

		def Load(self):
			self.shadowList = self.config.GetChild("ShadowSelectBar")
			for i in range(len(self.SHADOW_OPTIONS)):
				self.shadowList.InsertItem(i, self.SHADOW_OPTIONS[i])

			self.shadowList.SAFE_SetEvent(self.OnShadowSelect)
			self.shadowList.SelectItem(systemSetting.GetShadowLevel())

		def IsChanged(self):
			return self.dest_shadow != systemSetting.GetShadowLevel()

		def OnShadowSelect(self, index):
			self.shadowList.SetCurrentItem(self.SHADOW_OPTIONS[index])
			self.dest_shadow = index

	class SoundConfig:
		def __init__(self, config):
			self.config = config
			self.Load()

		def Destroy(self):
			self.config = None
			self.musicSlider = None
			self.sfxSlider = None

		def Load(self):
			self.musicSlider = self.config.GetChild("BGMSlider")
			self.sfxSlider = self.config.GetChild("SFXSlider")

			self.musicSlider.SetSliderPos(float(systemSetting.GetMusicVolume()))
			self.musicSlider.SetEvent(ui.__mem_func__(self.OnChangeMusicVolume))

			self.sfxSlider.SetSliderPos(float(systemSetting.GetSoundVolume()) / 5.0)
			self.sfxSlider.SetEvent(ui.__mem_func__(self.OnChangeSoundVolume))

		def OnChangeMusicVolume(self):
			pos = self.musicSlider.GetSliderPos()
			snd.SetMusicVolume(pos * net.GetFieldMusicVolume())
			systemSetting.SetMusicVolume(pos)

		def OnChangeSoundVolume(self):
			pos = self.sfxSlider.GetSliderPos()
			snd.SetSoundVolumef(pos)
			systemSetting.SetSoundVolumef(pos)


	class TextureConfig:
		TEXTURE_OPTIONS = [uiScriptLocale.CONFIG_TEXTURE_0,
							uiScriptLocale.CONFIG_TEXTURE_1,
							uiScriptLocale.CONFIG_TEXTURE_2,
							uiScriptLocale.CONFIG_TEXTURE_3]

		def __init__(self, config):
			self.config = config
			self.dest_texture = systemSetting.GetTextureViewType()
			self.Load()

		def Destroy(self):
			self.config = None
			self.textureList = None

		def Load(self):
			self.textureList = self.config.GetChild("TextureSelectBar")
			for i in range(len(self.TEXTURE_OPTIONS)):
				self.textureList.InsertItem(i, self.TEXTURE_OPTIONS[i])

			self.textureList.SAFE_SetEvent(self.OnTextureSelect)
			self.textureList.SelectItem(systemSetting.GetTextureViewType())

		def IsChanged(self):
			return self.dest_texture != systemSetting.GetTextureViewType()

		def OnTextureSelect(self, index):
			self.textureList.SetCurrentItem(self.TEXTURE_OPTIONS[index])
			self.dest_texture = index

	class LanguageConfig:
		# Dostepne locale znajdujace sie w locale/locale/
		LANGUAGE_OPTIONS = [
			"Polski",
			"English",
			"Deutsch",
			"Espanol",
			"Italiano",
			"Portugues",
			"Romana",
			"Turkce",
		]

		def __init__(self, config):
			self.config = config
			self.dest_language = systemSetting.GetLanguage()
			self.Load()

		def Destroy(self):
			self.config = None
			self.languageList = None

		def __IndexToLanguageCode(self, index):
			language_codes = ["pl", "en", "de", "es", "it", "pt", "ro", "tr"]
			if index < 0 or index >= len(language_codes):
				return "pl"
			return language_codes[index]

		def __LanguageCodeToIndex(self, lang):
			language_codes = ["pl", "en", "de", "es", "it", "pt", "ro", "tr"]
			try:
				return language_codes.index(lang)
			except ValueError:
				return 0

		def Load(self):
			self.languageList = self.config.GetChild("LanguageSelectBar")
			for i in range(len(self.LANGUAGE_OPTIONS)):
				self.languageList.InsertItem(i, self.LANGUAGE_OPTIONS[i])

			self.languageList.SAFE_SetEvent(self.OnLanguageSelect)

			index = self.__LanguageCodeToIndex(systemSetting.GetLanguage())
			self.languageList.SelectItem(index)

		def IsChanged(self):
			return self.dest_language != systemSetting.GetLanguage()

		def OnLanguageSelect(self, index):
			self.languageList.SetCurrentItem(self.LANGUAGE_OPTIONS[index])
			self.dest_language = self.__IndexToLanguageCode(index)


	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()
		self.LoadDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __Initialize(self):
		self.resolution_config = None
		self.custom_resolution_config = None
		self.screen_mode_config = None
		self.scale_interface_config = None
		self.questionDialog = None
		self.needs_restart = False

	def Destroy(self):
		if self.resolution_config:
			self.resolution_config.Destroy()

		if self.custom_resolution_config:
			self.custom_resolution_config.Destroy()

		if self.screen_mode_config:
			self.screen_mode_config.Destroy()

		if self.scale_interface_config:
			self.scale_interface_config.Destroy()

		if self.font_config:
			self.font_config.Destroy()

		if self.font_config:
			self.font_config.Destroy()

		if self.sampling_config:
			self.sampling_config.Destroy()

		if self.shadow_config:
			self.shadow_config.Destroy()

		if self.sound_config:
			self.sound_config.Destroy()

		if self.lang_config:
			self.lang_config.Destroy()

		self.ClearDictionary()
		self.__Initialize()

	def LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/configwindow.py")

			self.board = self.GetChild("Board")
			self.saveButton = self.GetChild("SaveButton")
			self.board.SetCloseEvent(ui.__mem_func__(self.Close))
			self.saveButton.SAFE_SetEvent(self.OnSaveButtonClick)

			self.scale_interface_config = self.ScaleInterfaceConfig(self)
			self.resolution_config = self.ResolutionConfig(self)
			self.custom_resolution_config = self.CustomResolutionConfig(self)
			self.screen_mode_config = self.ScreenModeConfig(self)
			self.font_config = self.FontConfig(self)
			self.sampling_config = self.MultiSamplingConfig(self)
			self.shadow_config = self.ShadowConfig(self)
			self.sound_config = self.SoundConfig(self)
			self.texture_config = self.TextureConfig(self)
			self.lang_config = self.LanguageConfig(self)

		except:
			import exception
			exception.Abort("ConfigWindow.LoadDialog.BindObject")

	def GetResolution(self):
		if self.resolution_config and self.resolution_config.IsChanged():
			return self.resolution_config.dest_width, self.resolution_config.dest_height
		elif self.custom_resolution_config and self.custom_resolution_config.IsChanged():
			return self.custom_resolution_config.GetResolution()
		else:
			return systemSetting.GetWidth(), systemSetting.GetHeight()

	def OnSaveButtonClick(self):
		self.needs_restart = False
		if self.resolution_config.IsChanged():
			self.needs_restart = True
		elif self.custom_resolution_config.IsChanged():
			self.needs_restart = True
		elif self.screen_mode_config.IsChanged():
			self.needs_restart = True
		elif self.scale_interface_config.IsChanged():
			self.needs_restart = True
		elif self.font_config.IsChanged():
			self.needs_restart = True
		elif self.font_config.IsChanged():
			self.needs_restart = True
		elif self.sampling_config.IsChanged():
			self.needs_restart = True
		elif self.texture_config.IsChanged():
			self.needs_restart = True
		elif self.lang_config.IsChanged():
			self.needs_restart = True

		if self.needs_restart:
			self.__OpenQuestionDialog()
		else:
			self.Save()

	def __OpenQuestionDialog(self):
		if self.questionDialog:
			self.questionDialog.Close()
			self.questionDialog = None

		dlg = uiCommon.QuestionDialog()
		dlg.SetText(localeInfo.CONFIG_NEED_RESTART)
		dlg.SAFE_SetAcceptEvent(self.Save)
		dlg.SetDefaultCancelEvent()
		dlg.SetParent(self)
		dlg.Open()
		self.questionDialog = dlg

	def Save(self):
		if self.resolution_config.IsChanged():
			systemSetting.SetResolution(self.resolution_config.dest_width, self.resolution_config.dest_height, self.resolution_config.dest_bpp)
			systemSetting.SetCustomResolution(0, 0)
		elif self.custom_resolution_config.IsChanged():
			width, height = self.custom_resolution_config.GetResolution()
			systemSetting.SetCustomResolution(width, height)

		if self.screen_mode_config.IsChanged():
			systemSetting.SetWindowed(self.screen_mode_config.is_windowed)

		if self.scale_interface_config.IsChanged():
			systemSetting.SetScaleInterface(self.scale_interface_config.scale)
			systemSetting.SetIsScaleInterface(self.scale_interface_config.is_scale_mode)

		if self.font_config.IsChanged():
			systemSetting.SetFont(self.font_config.dest_font, self.font_config.dest_size)

		if self.sampling_config.IsChanged():
			systemSetting.SetSamplingLevel(self.sampling_config.dest_sampling)

		if self.shadow_config.IsChanged():
			systemSetting.SetShadowLevel(self.shadow_config.dest_shadow)

		if self.texture_config.IsChanged():
			systemSetting.SetTextureViewType(self.texture_config.dest_texture)

		if self.lang_config.IsChanged():
			systemSetting.SetLanguage(self.lang_config.dest_language)

		systemSetting.SaveConfig()
		if self.needs_restart:
			app.Exit()
		else:
			self.Close()

	def Close(self):
		if self.questionDialog:
			self.questionDialog.Close()
			self.questionDialog = None

		self.Hide()

	def Open(self):
		self.needs_restart = False
		self.Show()
