import uiScriptLocale
import app
import flamewindPath

ROOT = "d:/ymir work/ui/game/"

Y_ADD_POSITION = 0

window = {
	"name" : "TaskBar",

	"x" : 0,
	"y" : SCREEN_HEIGHT - 37,

	"width" : SCREEN_WIDTH,
	"height" : 37,

	"children" :
	(
		## Board
		{
			"name" : "Base_Board_01",
			"type" : "expanded_image",

			"x" : 263,
			"y" : 0,

			"rect" : (0.0, 0.0, float(SCREEN_WIDTH - 263 - 256) / 256.0, 0.0),

			"image" : "d:/ymir work/ui/pattern/TaskBar_Base.tga"
		},

		## Gauge
		{
			"name" : "Gauge_Board",
			"type" : "image",

			"x" : 0,
			"y" : -10 + Y_ADD_POSITION,

			"image" : ROOT + "taskbar/gauge.sub",

			"children" :
			(
				{
					"name" : "RampageGauge",
					"type" : "ani_image",

					"x" : 8,
					"y" : 4,

					"delay" : 6,

					"images" :
					(
						ROOT + "TaskBar/Rampage_01/00.sub",
						ROOT + "TaskBar/Rampage_01/01.sub",
						ROOT + "TaskBar/Rampage_01/02.sub",
						ROOT + "TaskBar/Rampage_01/03.sub",
						ROOT + "TaskBar/Rampage_01/04.sub",
						ROOT + "TaskBar/Rampage_01/05.sub",
						ROOT + "TaskBar/Rampage_01/06.sub",
						ROOT + "TaskBar/Rampage_01/07.sub",
						ROOT + "TaskBar/Rampage_01/08.sub",
						ROOT + "TaskBar/Rampage_01/09.sub",
						ROOT + "TaskBar/Rampage_01/11.sub",
						ROOT + "TaskBar/Rampage_01/12.sub",
						ROOT + "TaskBar/Rampage_01/13.sub",
						ROOT + "TaskBar/Rampage_01/14.sub",
						ROOT + "TaskBar/Rampage_01/15.sub",
						ROOT + "TaskBar/Rampage_01/16.sub",
					)
				},

				{
					"name" :"ItemShopButton",
					"type" : "window",
					"x" : 8,
					"y" : 4,
					"width": 50,
					"height": 50,
				},
				{
					## Tooltip popup for
					"name" : "HPGauge_Board",
					"type" : "window",

					"x" : 59,
					"y" : 14,

					"width" : 95,
					"height" : 11,

					"children" :
					(
						{
							"name" : "HPRecoveryGaugeBar",
							"type" : "bar",

							"x" : 0,
							"y" : 0,
							"width" : 95,
							"height" : 13,
							"color" : 0x55ff0000,
						},
						{
							"name" : "HPGauge",
							"type" : "ani_image",

							"x" : 0,
							"y" : 0,

							"delay" : 6,

							"images" :
							(
								"D:/Ymir Work/UI/Pattern/HPGauge/01.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/02.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/03.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/04.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/05.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/06.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/07.tga",
							),
						},

						{
							"name" : "AbsorbDamageGauge",
							"type" : "ani_image",

							"x" : 0,
							"y" : 0,

							"delay" : 6,

							"images" :
							(
								"D:/Ymir Work/UI/Pattern/HPGauge/absorb/01.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/absorb/02.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/absorb/03.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/absorb/04.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/absorb/05.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/absorb/06.tga",
								"D:/Ymir Work/UI/Pattern/HPGauge/absorb/07.tga",
							),
						},
					),
				},
				{
					## Tooltip popup for
					"name" : "SPGauge_Board",
					"type" : "window",

					"x" : 59,
					"y" : 24,

					"width" : 95,
					"height" : 11,

					"children" :
					(
						{
							"name" : "SPRecoveryGaugeBar",
							"type" : "bar",

							"x" : 0,
							"y" : 0,
							"width" : 95,
							"height" : 13,
							"color" : 0x550000ff,
						},
						{
							"name" : "SPGauge",
							"type" : "ani_image",

							"x" : 0,
							"y" : 0,

							"delay" : 6,

							"images" :
							(
								"D:/Ymir Work/UI/Pattern/SPGauge/01.tga",
								"D:/Ymir Work/UI/Pattern/SPGauge/02.tga",
								"D:/Ymir Work/UI/Pattern/SPGauge/03.tga",
								"D:/Ymir Work/UI/Pattern/SPGauge/04.tga",
								"D:/Ymir Work/UI/Pattern/SPGauge/05.tga",
								"D:/Ymir Work/UI/Pattern/SPGauge/06.tga",
								"D:/Ymir Work/UI/Pattern/SPGauge/07.tga",
							),
						},
					),
				},
				{
					## Tooltip popup for
					"name" : "STGauge_Board",
					"type" : "window",

					"x" : 59,
					"y" : 38,

					"width" : 95,
					"height" : 6,

					"children" :
					(
						{
							"name" : "STGauge",
							"type" : "ani_image",

							"x" : 0,
							"y" : 0,

							"delay" : 6,

							"images" :
							(
								"D:/Ymir Work/UI/Pattern/STGauge/01.tga",
								"D:/Ymir Work/UI/Pattern/STGauge/02.tga",
								"D:/Ymir Work/UI/Pattern/STGauge/03.tga",
								"D:/Ymir Work/UI/Pattern/STGauge/04.tga",
								"D:/Ymir Work/UI/Pattern/STGauge/05.tga",
								"D:/Ymir Work/UI/Pattern/STGauge/06.tga",
								"D:/Ymir Work/UI/Pattern/STGauge/07.tga",
							),
						},
					),
				},

			),
		},
		{
			"name" : "EXP_Gauge_Board",
			"type" : "image",

			"x" : 158,
			"y" : 0 + Y_ADD_POSITION,

			"image" : ROOT + "taskbar/exp_gauge.sub",

			"children" :
			(
				{
					"name" : "EXPGauge_01",
					"type" : "expanded_image",

					"x" : 5,
					"y" : 9,

					"image" : ROOT + "TaskBar/EXP_Gauge_Point.sub",
				},
				{
					"name" : "EXPGauge_02",
					"type" : "expanded_image",

					"x" : 30,
					"y" : 9,

					"image" : ROOT + "TaskBar/EXP_Gauge_Point.sub",
				},
				{
					"name" : "EXPGauge_03",
					"type" : "expanded_image",

					"x" : 55,
					"y" : 9,

					"image" : ROOT + "TaskBar/EXP_Gauge_Point.sub",
				},
				{
					"name" : "EXPGauge_04",
					"type" : "expanded_image",

					"x" : 80,
					"y" : 9,

					"image" : ROOT + "TaskBar/EXP_Gauge_Point.sub",
				},
			),
		},

		## Mouse Button
		{
			"name" : "LeftMouseButton",
			"type" : "button",

			"x" : SCREEN_WIDTH/2 - 136,
			"y" : 3 + Y_ADD_POSITION,

			"default_image" : ROOT + "TaskBar/Mouse_Button_Move_01.sub",
			"over_image" : ROOT + "TaskBar/Mouse_Button_Move_02.sub",
			"down_image" : ROOT + "TaskBar/Mouse_Button_Move_03.sub",
		},
		{
			"name" : "RightMouseButton",
			"type" : "button",

			"x" : SCREEN_WIDTH/2 + 128 + 66 + 11 + 24,
			"y" : 3 + Y_ADD_POSITION,

			"default_image" : ROOT + "TaskBar/Mouse_Button_Move_01.sub",
			"over_image" : ROOT + "TaskBar/Mouse_Button_Move_02.sub",
			"down_image" : ROOT + "TaskBar/Mouse_Button_Move_03.sub",
		},

		## Button
		{
			"name" : "CharacterButton",
			"type" : "button",

			"x" : SCREEN_WIDTH - 137,
			"y" : 3 + Y_ADD_POSITION,

			"tooltip_text" : uiScriptLocale.TASKBAR_CHARACTER,

			"default_image" : ROOT + "TaskBar/Character_Button_01.sub",
			"over_image" : ROOT + "TaskBar/Character_Button_02.sub",
			"down_image" : ROOT + "TaskBar/Character_Button_03.sub",
		},
		{
			"name" : "InventoryButton",
			"type" : "button",

			"x" : SCREEN_WIDTH - 103,
			"y" : 3 + Y_ADD_POSITION,

			"tooltip_text" : uiScriptLocale.TASKBAR_INVENTORY,

			"default_image" : ROOT + "TaskBar/Inventory_Button_01.sub",
			"over_image" : ROOT + "TaskBar/Inventory_Button_02.sub",
			"down_image" : ROOT + "TaskBar/Inventory_Button_03.sub",
		},
		{
			"name" : "MessengerButton",
			"type" : "button",

			"x" : SCREEN_WIDTH - 69,
			"y" : 3 + Y_ADD_POSITION,

			"tooltip_text" : uiScriptLocale.TASKBAR_MESSENGER,

			"default_image" : ROOT + "TaskBar/Community_Button_01.sub",
			"over_image" : ROOT + "TaskBar/Community_Button_02.sub",
			"down_image" : ROOT + "TaskBar/Community_Button_03.sub",
		},
		{
			"name" : "SystemButton",
			"type" : "button",

			"x" : SCREEN_WIDTH - 35,
			"y" : 3 + Y_ADD_POSITION,

			"tooltip_text" : uiScriptLocale.TASKBAR_SYSTEM,

			"default_image" : ROOT + "TaskBar/System_Button_01.sub",
			"over_image" : ROOT + "TaskBar/System_Button_02.sub",
			"down_image" : ROOT + "TaskBar/System_Button_03.sub",
		},

		## QuickBar
		{
			"name" : "quickslot_board",
			"type" : "window",

			"x" : SCREEN_WIDTH/2 - 128 + 32 + 10 - 16,
			"y" : 0 + Y_ADD_POSITION,

			"width" : 320 + 11,
			"height" : 37,

			"children" :
			(
				# {
				# 	"name" : "ExpandButton",
				# 	"type" : "button",
				#
				# 	"x" : 128,
				# 	"y" : 1,
				# 	"tooltip_text" : uiScriptLocale.TASKBAR_EXPAND,
				#
				# 	"default_image" : ROOT + "TaskBar/Chat_Button_01.sub",
				# 	"over_image" : ROOT + "TaskBar/Chat_Button_02.sub",
				# 	"down_image" : ROOT + "TaskBar/Chat_Button_03.sub",
				# },
				{
					"name" : "quick_slot_1",
					"type" : "grid_table",

					"start_index" : 0,

					"x" : 0,
					"y" : 3,

					"x_count" : 6,
					"y_count" : 1,
					"x_step" : 32,
					"y_step" : 32,

					"image" : "d:/ymir work/ui/Public/Slot_Base.sub",
					"image_r" : 1.0,
					"image_g" : 1.0,
					"image_b" : 1.0,
					"image_a" : 1.0,

					"children" :
					(
						{ "name" : "slot_1", "type" : "image", "x" : 3, "y" : 3, "image" : "d:/ymir work/ui/game/taskbar/1.sub", },
						{ "name" : "slot_2", "type" : "image", "x" : 35, "y" : 3, "image" : "d:/ymir work/ui/game/taskbar/2.sub", },
						{ "name" : "slot_3", "type" : "image", "x" : 67, "y" : 3, "image" : "d:/ymir work/ui/game/taskbar/3.sub", },
						{ "name" : "slot_4", "type" : "image", "x" : 99, "y" : 3, "image" : "d:/ymir work/ui/game/taskbar/4.sub", },
						{ "name" : "slot_5", "type" : "image", "x" : 131, "y" : 3, "image" : "d:/ymir work/ui/game/taskbar/5.sub", },
						{ "name" : "slot_6", "type" : "image", "x" : 163, "y" : 3, "image" : flamewindPath.GetPublic("tab_key"), },
					),
				},
				{
					"name" : "quick_slot_2",
					"type" : "grid_table",

					"start_index" : 6,

					"x" : 192,
					"y" : 3,

					"x_count" : 4,
					"y_count" : 1,
					"x_step" : 32,
					"y_step" : 32,

					"image" : "d:/ymir work/ui/Public/Slot_Base.sub",
					"image_r" : 1.0,
					"image_g" : 1.0,
					"image_b" : 1.0,
					"image_a" : 1.0,

					"children" :
					(
						{ "name" : "slot_7", "type" : "image", "x" : 3, "y" : 3, "image" : "d:/ymir work/ui/game/taskbar/f1.sub", },
						{ "name" : "slot_8", "type" : "image", "x" : 35, "y" : 3, "image" : "d:/ymir work/ui/game/taskbar/f2.sub", },
						{ "name" : "slot_9", "type" : "image", "x" : 67, "y" : 3, "image" : "d:/ymir work/ui/game/taskbar/f3.sub", },
						{ "name" : "slot_10", "type" : "image", "x" : 99, "y" : 3, "image" : "d:/ymir work/ui/game/taskbar/f4.sub", },
					),
				},
				{
					"name" : "QuickSlotBoard",
					"type" : "window",

					"x" : 160+14+128+2+16,
					"y" : 0,
					"width" : 11,
					"height" : 37,
					"children" :
					(
						{
							"name" : "QuickSlotNumberBox",
							"type" : "image",
							"x" : 1,
							"y" : 15,
							"image" : ROOT + "taskbar/QuickSlot_Button_Board.sub",
						},
						{
							"name" : "QuickPageUpButton",
							"type" : "button",
							"tooltip_text" : uiScriptLocale.TASKBAR_PREV_QUICKSLOT,
							"x" : 1,
							"y" : 9,
							"default_image" : ROOT + "TaskBar/QuickSlot_UpButton_01.sub",
							"over_image" : ROOT + "TaskBar/QuickSlot_UpButton_02.sub",
							"down_image" : ROOT + "TaskBar/QuickSlot_UpButton_03.sub",
						},

						{
							"name" : "QuickPageNumber",
							"type" : "image",
							"x" : 3, "y" : 15, "image" : "d:/ymir work/ui/game/taskbar/1.sub",
						},
						{
							"name" : "QuickPageDownButton",
							"type" : "button",
							"tooltip_text" : uiScriptLocale.TASKBAR_NEXT_QUICKSLOT,

							"x" : 1,
							"y" : 24,

							"default_image" : ROOT + "TaskBar/QuickSlot_DownButton_01.sub",
							"over_image" : ROOT + "TaskBar/QuickSlot_DownButton_02.sub",
							"down_image" : ROOT + "TaskBar/QuickSlot_DownButton_03.sub",
						},

					),
				},
			),
		},

	),
}

# Towarzysz and Auto Lowy (uisidekick.py, uiautohunt.py), when the bar has
# room for them - clientrootify.py.
if SCREEN_WIDTH >= 940:
	window["children"] = window["children"] + (
		{
			"name" : "SidekickButton",
			"type" : "button",

			"x" : SCREEN_WIDTH - 205,
			"y" : 3 + Y_ADD_POSITION,

			"tooltip_text" : "Towarzysz (P)",

			"default_image" : "playerbot_ui/sidekick_button_01.tga",
			"over_image" : "playerbot_ui/sidekick_button_02.tga",
			"down_image" : "playerbot_ui/sidekick_button_03.tga",
		},
		{
			"name" : "AutoHuntButton",
			"type" : "button",

			"x" : SCREEN_WIDTH - 171,
			"y" : 3 + Y_ADD_POSITION,

			"tooltip_text" : "Auto \xa3owy (K)",

			"default_image" : "playerbot_ui/autohunt_button_01.tga",
			"over_image" : "playerbot_ui/autohunt_button_02.tga",
			"down_image" : "playerbot_ui/autohunt_button_03.tga",
		},
	)
