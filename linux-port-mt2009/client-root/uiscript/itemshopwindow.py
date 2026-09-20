import uiScriptLocale
import flamewindPath
import colorInfo

WINDOW_WIDTH = 662
WINDOW_HEIGHT = 464

CENTER_X = SCREEN_WIDTH/2 - WINDOW_WIDTH/2
CENTER_Y = SCREEN_HEIGHT/2 - WINDOW_HEIGHT/2

CONTENT_PANEL_WIDTH = WINDOW_WIDTH - 29 - 160
CONTENT_PANEL_HEIGHT = WINDOW_HEIGHT - 30 - 12

window = {
	"name" : "ItemShopDialog",

	"x" : CENTER_X,
	"y" : CENTER_Y,
	
	"style": ("movable", "float",),

	"width" : WINDOW_WIDTH,
	"height" : WINDOW_HEIGHT,

	"children" :
	(
		{
			"name" : "board",
			"type" : "board",
			"style" : ("attach",),

			"x" : 0,
			"y" : 0,

			"width" : WINDOW_WIDTH,
			"height" : WINDOW_HEIGHT,

			"children" :
			(
				{
					"name": "TitleBar",
					"type": "titlebar",
					"style": ("attach",),

					"x": 8,
					"y": 8,

					"width": WINDOW_WIDTH - 15,
					"color": "gray",

					"children":
					(
						{
							"name": "TitleName", "type": "text", "x": -4, "y": 4, "text": "Sklep z przedmiotami",
							"horizontal_align": "center", "text_horizontal_align": "center"
						},

						{
							"name" : "RefreshButton",
							"type" : "button",

							"x" : 17+21,
							"y" : 2,

							"horizontal_align" : "right",
							"tooltip_text" : uiScriptLocale.REFRESH,

							"default_image" : flamewindPath.GetPublic("refresh_button_01"),
							"over_image" : flamewindPath.GetPublic("refresh_button_02"),
							"down_image" : flamewindPath.GetPublic("refresh_button_03"),
						},
					),
				},
				
				{
					"name": "ContentPanel",
					"type": "board",
					"x": 180,
					"y": 32,
					"width": CONTENT_PANEL_WIDTH,
					"height": CONTENT_PANEL_HEIGHT,
					
					"skin": "a",

					"children" : (
						{
							"name" : "Content",
							"type" : "window",
							"x" : 2,
							"y" : 2,
							"width" : CONTENT_PANEL_WIDTH - 4,
							"height" : CONTENT_PANEL_HEIGHT - 4,

							"children" : (
								{
									"type": "horizontalbar",
									"x": 2,
									"y": 2,
									"width": CONTENT_PANEL_WIDTH-4 - 4,
								},
							)
						},
					),
				},

				{
					"name": "MenuPanel",
					"type": "board",
					"x": 10,
					"y": 32,
					"width": 158,
					"height": WINDOW_HEIGHT - 30 - 12,
					
					"skin": "a",
					
					"children" : (
					
						# Title 1
						{
							"type" : "horizontalbar",

							"x" : 0,
							"y" : 5,

							"width" : 148,
							
							"children" : (
								{
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"all_align" : True,
									"color" : colorInfo.GetColorFromColorTuple(colorInfo.CHAT_RGB_NOTICE),
									"text" : uiScriptLocale.ITEMSHOP_ACCOUNT_STATE,
									"bold" : True,
									"fontsize" : "LARGE",
								},
							),

							"horizontal_align" : "center",
						},
						
						# SM Counter
						{
							"type" : "horizontalbar",

							"x" : -30,
							"y" : 37,

							"horizontal_align" : "center",
							
							"width" : 50,
							"skin" : "c",
							
							"children" : (
								{
									"name" : "DragonCoinAmount",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"all_align" : True,
									"text" : "0",
									"bold" : True,
								},
								
								{
									"type" : "text",
									"x" : 2,
									"y" : -13,
									"color" : colorInfo.GetColorFromColorTuple(colorInfo.DRAGON_COIN_COLOR),
									"text" : "SM",
									"bold" : True,
								},
							),
						},
						
						# SZ Counter
						{
							"type" : "horizontalbar",

							"x" : 30,
							"y" : 37,

							"horizontal_align" : "center",
							
							"width" : 50,
							"skin" : "c",
							
							"children" : (
								{
									"name" : "DragonMarkAmount",
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"all_align" : True,
									"text" : "0",
									"bold" : True,
								},
								
								{
									"type" : "text",
									"x" : 2,
									"y" : -13,
									"color" : colorInfo.GetColorFromColorTuple(colorInfo.DRAGON_MARK_COLOR),
									"text" : "SZ",
									"bold" : True,
								},
							),
						},
						
						# Title 2
						{
							"type" : "horizontalbar",

							"x" : 0,
							"y" : 65,

							"width" : 148,
							
							"children" : (
								{
									"type" : "text",
									"x" : 0,
									"y" : 0,
									"all_align" : True,
									"color" : colorInfo.GetColorFromColorTuple(colorInfo.CHAT_RGB_NOTICE),
									"text" : uiScriptLocale.ITEMSHOP_CATEGORIES,
									"bold" : True,
									"fontsize" : "LARGE",
								},
							),

							"horizontal_align" : "center",
						},
					
						# {
							# "type" : "button",

							# "x" : 0,
							# "y" : 88,

							# "horizontal_align" : "center",

							# "default_image" : flamewindPath.GetPublic("big_button_01"),
							# "over_image" : flamewindPath.GetPublic("big_button_02"),
							# "down_image" : flamewindPath.GetPublic("big_button_03"),
						# },
					),
				},

				{
					"name" : "BuyQuestion",
					"type" : "bar",
					"x" : 6,
					"y" : 30,

					"width": WINDOW_WIDTH - 12,
					"height": WINDOW_HEIGHT - 37,

					"color" : 0xb5000000,

					"children" : (
						{
							"name": "BuyQuestionBoard",
							"type" : "board",
							"x" : 0,
							"y" : 0,
							"width" : 260,
							"height" : 160,

							"horizontal_align" : "center",
							"vertical_align" : "center",

							"children" : (
								{
									"type" : "text",
									"x": 0,
									"y": 10,

									"horizontal_align" : "center",
									"text_horizontal_align" : "center",
									"bold" : True,
									"fontsize" : "LARGE",

									"text" : "Potwierdzenie zakupu",
								},

								{
									"name": "ItemSlot",
									"type": "image",
									"x": 16,
									"y": 32,

									"image": "d:/ymir work/ui/public/slot_base.sub",
									"children" : (
										{
											"name" : "ItemIcon",
											"type" : "image",
											"x": 0,
											"y": 0,
											"image" : "icon/item/71084.tga",
										},
									),
								},

								{
									"name" : "ItemName",
									"type" : "text",
									"x": 16+40,
									"y": 32+3,
									"bold" : True,
									"text" : "ITEM_NAME",
								},
								{
									"name" : "ItemCount",
									"type" : "text",
									"x": 16+40,
									"y": 32+16,
									"text" : "ITEM_COUNT",
								},

								{
									"name": "Buy1",
									"type": "button",

									"x": 0,
									"y": 88,

									"horizontal_align": "center",
									"vertical_align": "bottom",

									"text": "Kup 1",

									"default_image": "d:/ymir work/ui/public/xlarge_button_01.sub",
									"over_image": "d:/ymir work/ui/public/xlarge_button_02.sub",
									"down_image": "d:/ymir work/ui/public/xlarge_button_03.sub",
								},
								{
									"name": "Buy5",
									"type": "button",

									"x": 0,
									"y": 63,

									"horizontal_align": "center",
									"vertical_align": "bottom",

									"text": "Kup 5",

									"default_image": "d:/ymir work/ui/public/xlarge_button_01.sub",
									"over_image": "d:/ymir work/ui/public/xlarge_button_02.sub",
									"down_image": "d:/ymir work/ui/public/xlarge_button_03.sub",
								},

								{
									"name": "Buy10",
									"type": "button",

									"x": 0,
									"y": 38,

									"horizontal_align": "center",
									"vertical_align": "bottom",

									"text": "Kup 10",

									"default_image": "d:/ymir work/ui/public/xlarge_button_01.sub",
									"over_image": "d:/ymir work/ui/public/xlarge_button_02.sub",
									"down_image": "d:/ymir work/ui/public/xlarge_button_03.sub",
								},
							),
						},
					),
				}
			),
		},
	),
}