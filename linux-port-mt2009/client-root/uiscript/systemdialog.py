import app
import uiScriptLocale
from utils import ElementAddBefore, FindElementRef

BUTTON_STEP = 30

ROOT = "d:/ymir work/ui/public/"

window = {
	"name" : "SystemDialog",
	"style" : ("float",),

	"x" : (SCREEN_WIDTH - 200) /2,
	"y" : (SCREEN_HEIGHT - (378 - 30)) /2,

	"width" : 200,
	"height" : 318,

	"children" :
	[
		{
			"name" : "board",
			"type" : "thinboard",

			"x" : 0,
			"y" : 0,

			"width" : 200,
			"height" : 318,

			"children" :
			[
				{
					"name" : "help_button",
					"type" : "button",

					"x" : 10,
					"y" : 17,

					"text" : uiScriptLocale.SYSTEM_HELP,

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
				{
					"name" : "support_button",
					"type" : "button",

					"x" : 10,
					"y" : 57,

					"text" : "Support",

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
				{
					"name" : "GameVersion",
					"type" : "text",

					"x" : 0,
					"y" : 92,

					"text" : "Support",
					"horizontal_align" : "center",
					"text_horizontal_align" : "center",
					"bold": True,

					"text": uiScriptLocale.SYSTEM_VERSION % (1, 0, 0, ""),
				},
				{
					"name" : "mall_button",
					"type" : "button",

					"x" : 10,
					"y" : 112,

					"text" : uiScriptLocale.SYSTEM_MALL,
					"text_color" : 0xffF8BF24,

					"default_image" : ROOT + "XLarge_Button_02.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_02.sub",
				},

				{
					"name" : "system_option_button",
					"type" : "button",

					"x" : 10,
					"y" : 112 + BUTTON_STEP,

					"text" : uiScriptLocale.SYSTEMOPTION_TITLE,

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
				{
					"name" : "game_option_button",
					"type" : "button",

					"x" : 10,
					"y" : 112 + BUTTON_STEP * 2,

					"text" : uiScriptLocale.GAMEOPTION_TITLE,

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
				{
					"name" : "escape_button",
					"type" : "button",

					"x" : 10,
					"y" : 112 + BUTTON_STEP * 3,

					"text" : uiScriptLocale.SYSTEM_ESCAPE,

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
				{
					"name" : "change_button",
					"type" : "button",

					"x" : 10,
					"y" : 112 + BUTTON_STEP * 3,

					"text" : uiScriptLocale.SYSTEM_CHANGE,

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
				{
					"name" : "logout_button",
					"type" : "button",

					"x" : 10,
					"y" : 112 + BUTTON_STEP * 5,

					"text" : uiScriptLocale.SYSTEM_LOGOUT,

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
				{
					"name" : "exit_button",
					"type" : "button",

					"x" : 10,
					"y" : 112 + BUTTON_STEP * 6,

					"text" : uiScriptLocale.SYSTEM_EXIT,

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
				{
					"name" : "cancel_button",
					"type" : "button",

					"x" : 10,
					"y" : 112 + BUTTON_STEP * 7,

					"text" : uiScriptLocale.CANCEL,

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				},
			],
		},
	],
}

#############################################################################
MAIN_BOARD = FindElementRef(window["children"], "board")

if app.ENABLE_MOVE_CHANNEL:
	ElementAddBefore(MAIN_BOARD["children"], "logout_button",
				{
					"name" : "movechannel_button",
					"type" : "button",

					"x" : 10,
					"y" : -1,

					"text" : uiScriptLocale.SYSTEM_MOVE_CHANNEL,

					"default_image" : ROOT + "XLarge_Button_01.sub",
					"over_image" : ROOT + "XLarge_Button_02.sub",
					"down_image" : ROOT + "XLarge_Button_03.sub",
				})

def Recalculate():
	global MAIN_BOARD
	# incrementor
	INIT_Y = 10
	def Next():
		ret = getattr(Recalculate, "NEXT_Y", INIT_Y)
		setattr(Recalculate, "NEXT_Y", ret + 30)
		return ret
	# update Y of every element
	for elem in MAIN_BOARD["children"]:
		if elem["name"] == "GameVersion":
			elem["y"] = Next() + 5
		else:
			elem["y"] = Next()
	# update height
	window["height"] = MAIN_BOARD["height"] = Next() + INIT_Y

Recalculate()
