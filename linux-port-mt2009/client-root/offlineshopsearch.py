import ikashop
import constInfo
import ui
import eventManager
import localeInfo
import wndMgr
import player
import chat
import uiPrivateShopBuilder
from _weakref import proxy

SHOP_SEARCH_FILTERS = {
	ikashop.SHOP_SEARCH_CATEGORY_BOOKS: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_BOOKS,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_WARRIOR_0: {
				"name": localeInfo.SHOP_SEARCH_SUBCATEGORY_SKILL_WARRIOR_BODY,
				"itemList": [
					(50300, 1), (50300, 2), (50300, 3), (50300, 4), (50300, 5),
					(50401, 0), (50402, 0), (50403, 0), (50404, 0), (50405, 0),
					(70037, 1), (70037, 2), (70037, 3), (70037, 4), (70037, 5),  # oz body
				]
			},

			ikashop.SHOP_SEARCH_SUB_WARRIOR_1: {
				"name": localeInfo.SHOP_SEARCH_SUBCATEGORY_SKILL_WARRIOR_MENTAL,
				"itemList": [
					(50300, 16), (50300, 17), (50300, 18), (50300, 19), (50300, 20),
					(50416, 0), (50417, 0), (50418, 0), (50419, 0), (50420, 0),
					(70037, 16), (70037, 17), (70037, 18), (70037, 19), (70037, 20),  # oz mental
				]
			},

			ikashop.SHOP_SEARCH_SUB_ASSASSIN_0: {
				"name": localeInfo.SHOP_SEARCH_SUBCATEGORY_SKILL_ASSASSIN_DAGGER,
				"itemList": [
					(50300, 31), (50300, 32), (50300, 33), (50300, 34), (50300, 35),
					(50431, 0), (50432, 0), (50433, 0), (50434, 0), (50435, 0),
					(70037, 31), (70037, 32), (70037, 33), (70037, 34), (70037, 35),  # oz dagger
				]
			},

			ikashop.SHOP_SEARCH_SUB_ASSASSIN_1: {
				"name": localeInfo.SHOP_SEARCH_SUBCATEGORY_SKILL_ASSASSIN_ARCHER,
				"itemList": [
					(50300, 46), (50300, 47), (50300, 48), (50300, 49), (50300, 50),
					(50446, 0), (50447, 0), (50448, 0), (50449, 0), (50450, 0),
					(70037, 46), (70037, 47), (70037, 48), (70037, 49), (70037, 50),  # oz archer
				]
			},

			ikashop.SHOP_SEARCH_SUB_SURA_0: {
				"name": localeInfo.SHOP_SEARCH_SUBCATEGORY_SKILL_SURA_WP,
				"itemList": [
					(50300, 61), (50300, 62), (50300, 63), (50300, 64), (50300, 65), (50300, 66),
					(50461, 0), (50462, 0), (50463, 0), (50464, 0), (50465, 0), (50466, 0),
					(70037, 61), (70037, 62), (70037, 63), (70037, 64), (70037, 65), (70037, 66),  # oz wp
				]
			},

			ikashop.SHOP_SEARCH_SUB_SURA_1: {
				"name": localeInfo.SHOP_SEARCH_SUBCATEGORY_SKILL_SURA_BM,
				"itemList": [
					(50300, 76), (50300, 77), (50300, 78), (50300, 79), (50300, 80), (50300, 81),
					(50476, 0), (50477, 0), (50478, 0), (50479, 0), (50480, 0), (50481, 0),
					(70037, 76), (70037, 77), (70037, 78), (70037, 79), (70037, 80), (70037, 81),  # oz bm
				]
			},

			ikashop.SHOP_SEARCH_SUB_SHAMAN_0: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_SKILL_SHAMAN_DRAGON,
				"itemList": [
					(50300, 91), (50300, 92), (50300, 93), (50300, 94), (50300, 95), (50300, 96),
					(50491, 0), (50492, 0), (50493, 0), (50494, 0), (50495, 0), (50496, 0),
					(70037, 91), (70037, 92), (70037, 93), (70037, 94), (70037, 95), (70037, 96),  # oz smok
				]
			},

			ikashop.SHOP_SEARCH_SUB_SHAMAN_1: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_SKILL_SHAMAN_HEALER,
				"itemList": [
					(50300, 106), (50300, 107), (50300, 108), (50300, 109), (50300, 110), (50300, 111),
					(50506, 0), (50507, 0), (50508, 0), (50509, 0), (50510, 0), (50511, 0),
					(70037, 106), (70037, 107), (70037, 108), (70037, 109), (70037, 110), (70037, 111),  # oz healer
				]
			},

			ikashop.SHOP_SEARCH_SUB_PASSIVE_SKILL: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_PASSIVE_SKILLS,
				"itemList": [
					(50600, 0),(50301, 0),(50302, 0),(50303, 0),(50304, 0),(50305, 0),(50306, 0),(50311, 0),(50312, 0),(50313, 0),(50314, 0),(50315, 0),(50316, 0),
				]
			},
		},
	},

	ikashop.SHOP_SEARCH_CATEGORY_REFINE: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_REFINE_M1: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_M1,
				"itemList": [
					(30003, 0), (30004, 0), (30010, 0), (30023, 0), (30027, 0), (30028, 0), (30037, 0), (30038, 0), (30053, 0),
					(30069, 0), (30070, 0), (30071, 0), (30072, 0), (30346, 0), (30347, 0), (30353, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_REFINE_OATH: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_OATH,
				"itemList": [
					(30011, 0), (30017, 0), (30018, 0), (30031, 0), (30034, 0), (30035, 0), (30073, 0), (30356, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_REFINE_M2: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_M2,
				"itemList": [
					(30005, 0), (30021, 0), (30030, 0), (30032, 0), (30033, 0), (30041, 0), (30052, 0), (30074, 0), (30075, 0),
					(30092, 0), (30357, 0), (30358, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_REFINE_ORC: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_ORC,
				"itemList": [
					(30007, 0), (30076, 0), (30352, 0), (30006, 0), (30077, 0), (30008, 0), (30078, 0), (30051, 0), (30079, 0),
					(30047, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_REFINE_DESERT1: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_DESERT1,
				"itemList": [
					(30022, 0), (30025, 0), (30045, 0), (30046, 0), (30081, 0), (30055, 0), (30056, 0), (30057, 0), (30058, 0),
					(30059, 0), (30067, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_REFINE_DESERT2: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_DESERT2,
				"itemList": [
					(30350, 0), (30351, 0), (30359, 0), (30082, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_REFINE_SNOW: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_MOUNTAIN,
				"itemList": [
					(30009, 0), (30014, 0), (30039, 0), (30048, 0), (30049, 0), (30050, 0), (30083, 0), (30085, 0), (30088, 0),
					(30089, 0), (30090, 0), (30348, 0), (30355, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_REFINE_HWANG: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_HWANG,
				"itemList": [
					(30040, 0), (30060, 0), (30061, 0), (30080, 0), (30344, 0), (30345, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_REFINE_END: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_END,
				"itemList": [
					(30019, 0), (30042, 0), (30091, 0), (30367, 0), (30354, 0), (30349, 0), (30015, 0), (30087, 0), (30016, 0),
					(30086, 0), (30343, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_REFINE_SPECIAL: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_SPECIAL,
				"itemList": [
					(30371, 0), (30372, 0), (30373, 0), (30374, 0), (30375, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_REFINE_PEARL: {
				"name": localeInfo.SHOP_SEARCH_CATEGORY_REFINE_PEARL,
				"itemList": [
					(27992, 0), (27993, 0), (27994, 0),
				]
			},
		},
	},

	ikashop.SHOP_SEARCH_CATEGORY_SOULSTONE : {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_SOUL_STONE,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_SOULSTONE_0: {
				"name": "+0",
				"itemList": [
					(28030, 0), (28031, 0), (28032, 0), (28033, 0), (28034, 0), (28035, 0), (28036, 0), (28037, 0), (28038, 0),
					(28039, 0), (28040, 0), (28041, 0), (28042, 0), (28043, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_SOULSTONE_1: {
				"name": "+1",
				"itemList": [
					(28130, 0), (28131, 0), (28132, 0), (28133, 0), (28134, 0), (28135, 0), (28136, 0), (28137, 0),
					(28138, 0), (28139, 0), (28140, 0), (28141, 0), (28142, 0), (28143, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_SOULSTONE_2: {
				"name": "+2",
				"itemList": [
					(28230, 0), (28231, 0), (28232, 0), (28233, 0), (28234, 0), (28235, 0), (28236, 0), (28237, 0),
					(28238, 0), (28239, 0), (28240, 0), (28241, 0), (28242, 0), (28243, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_SOULSTONE_3: {
				"name": "+3",
				"itemList": [
					(28330, 0), (28331, 0), (28332, 0), (28333, 0), (28334, 0), (28335, 0), (28336, 0), (28337, 0),
					(28338, 0), (28339, 0), (28340, 0), (28341, 0), (28342, 0), (28343, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_SOULSTONE_4: {
				"name": "+4",
				"itemList": [
					(28430, 0), (28431, 0), (28432, 0), (28433, 0), (28434, 0), (28435, 0), (28436, 0), (28437, 0),
					(28438, 0), (28439, 0), (28440, 0), (28441, 0), (28442, 0), (28443, 0),
				]
			},
		},
	},

	ikashop.SHOP_SEARCH_CATEGORY_HERBALISM: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_HERBALISM,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_HERB_PRIMARY: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_HERBS_PRIMARY,
				"itemList": [
					(50721, 0), (50722, 0), (50723, 0), (50724, 0), (50725, 0), (50726, 0), (50727, 0), (50728, 0), (50729, 0), (50730, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_HERB_SPECIAL: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_HERBS_SPECIAL,
				"itemList": [
					(50731, 0), (50732, 0), (50733, 0), (50734, 0), (50735, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_HERB_WATER_OFFENSIVE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_OFFENSIVE1,
				"itemList": [
					(51720, 0), (51725, 0), (51740, 0), (51745, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_HERB_JUICE_OFFENSIVE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_OFFENSIVE2,
				"itemList": [
					(51721, 0), (51726, 0), (51741, 0), (51746, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_HERB_DEW_OFFENSIVE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_OFFENSIVE3,
				"itemList": [
					(51722, 0), (51727, 0), (51742, 0), (51747, 0), (51748, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_HERB_WATER_DEFENSIVE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_DEFENSIVE1,
				"itemList": [
					(51730, 0), (51750, 0), (51800, 0), (51805, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_HERB_JUICE_DEFENSIVE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_DEFENSIVE2,
				"itemList": [
					(51731, 0), (51751, 0), (51801, 0), (51806, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_HERB_DEW_DEFENSIVE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_DEFENSIVE3,
				"itemList": [
					(51732, 0), (51752, 0), (51753, 0), (51802, 0), (51807, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_HERB_WATER_POWER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_POWER1,
				"itemList": [
					(27100, 0), (27103, 0), (51700, 0), (51705, 0), (51710, 0), (51715, 0),
					(51735, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_HERB_JUICE_POWER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_POWER2,
				"itemList": [
					(27101, 0), (27104, 0), (51701, 0), (51706, 0), (51711, 0), (51716, 0),
					(51736, 0), (51755, 0), (51760, 0), (51775, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_HERB_DEW_POWER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_POWER3,
				"itemList": [
					(27102, 0), (27105, 0), (51702, 0), (51707, 0), (51712, 0), (51717, 0),
					(51737, 0), (51756, 0), (51761, 0), (51776, 0), (51765, 0), (51766, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_HERB_OTHER_POTION: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_POTION_OTHER,
				"itemList": [
					(51782, 0), (51783, 0), (51784, 0), (51785, 0), (71153, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_HERB_AUTOPOTION: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_AUTOPOTION,
				"itemList": [
					(72723, 0), (72724, 0), (72725, 0), (72727, 0), (72728, 0), (72729, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_HERB_RECIPE_OFFENSIVE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEOGRY_RECIPE_OFFENSIVE,
				"itemList": [
					(50917, 0), (50918, 0), (50921, 0), (50922, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_HERB_RECIPE_DEFENSIVE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEOGRY_RECIPE_DEFENSIVE,
				"itemList": [
					(50919, 0), (50942, 0), (50943, 0), (50923, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_HERB_RECIPE_POWER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEOGRY_RECIPE_POWER,
				"itemList": [
					(50911, 0),
					(50912, 0),
					(50913, 0),
					(50914, 0),
					(50915, 0),
					(50916, 0),
					(50920, 0),
					(50924, 0),
					(50925, 0),
					(50926, 0),
					(50927, 0),
					(50929, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_HERB_RECIPE_OTHER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEOGRY_RECIPE_OTHER,
				"itemList": [
					(50930, 0), (50932, 0), (50933, 0), (50934, 0), (50935, 0),
				]
			},
		},
	},

	ikashop.SHOP_SEARCH_CATEGORY_FISHING: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_FISHING,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_FISHING_FISH: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_FISH,
				"itemList": [
					(27802, 0), (27803, 0), (27804, 0), (27805, 0), (27806, 0),
					(27807, 0), (27808, 0), (27809, 0), (27810, 0), (27811, 0), (27812, 0), (27813, 0), (27814, 0),
					(27815, 0), (27816, 0), (27818, 0), (27819, 0), (27820, 0), (27821, 0), (27822, 0), (27823, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_FISHING_FISH_COOK: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_FISH_COOK,
				"itemList": [
					(27866, 0), (27868, 0), (27869, 0), (27870, 0), (27871, 0), (27872, 0), (27873, 0), (27875, 0),
					(27879, 0), (27880, 0), (27881, 0), (27882, 0), (27883, 0),
				]
			},
			ikashop.SHOP_SEARCH_SUB_FISHING_FISH_OTHER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_FISH_OTHER,
				"itemList": [
					(70201, 0), (70007, 0), (70051, 0), (70050, 0), (70048, 0), (70049, 0), (70102, 0), (27798, 0), (27799, 0), (27987, 0),
				]
			},
		},
	},

	ikashop.SHOP_SEARCH_CATEGORY_MINING: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_MINING,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_MINING_ORE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_MINING_ORE,
				"itemList": [
					(50601, 0), (50603, 0), (50604, 0), (50605, 0), (50606, 0), (50607, 0), (50608, 0), (50609, 0), (50610, 0),
					(50611, 0), (50612, 0), (50613, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_MINING_MELT: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_MINING_SMELT,
				"itemList": [
					(50621, 0), (50623, 0), (50624, 0), (50625, 0), (50626, 0), (50627, 0),
					(50628, 0), (50629, 0), (50630, 0), (50631, 0), (50632, 0), (50633, 0),
				]
			},
		},
	},

	ikashop.SHOP_SEARCH_CATEGORY_HORSE: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_HORSE,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_HORSE_LEARN: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_HORSE_LEARN,
				"itemList": [
					(50050, 0), (50060, 0), (50061, 0), (50062, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_HORSE_OTHER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_HORSE_OTHER,
				"itemList": [
					(50054, 0), (50055, 0), (50056, 0), (50083, 0), (30378, 0),
				]
			},
		},
	},

	ikashop.SHOP_SEARCH_CATEGORY_POLYMORPH: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_POLYMARBLE,
		"itemList": [
			(70104, 0),
		]
	},

	ikashop.SHOP_SEARCH_CATEGORY_SPECIAL: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_SPECIAL_ITEMS,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_SPECIAL_REFINE: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_SPECIAL_REFINE,
				"itemList": [
					(25040, 0), (25043, 0), (25044, 0), (25045, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_SPECIAL_TOITEM: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_SPECIAL_TOITEM,
				"itemList": [
					(25100, 0), (30368, 0), (30370, 0), (71282, 0), (71284, 0), (71285, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_SPECIAL_CHARACTER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_SPECIAL_CHAR,
				"itemList": [
					(71201, 0),  (71294, 0), (70102, 0), (50513, 0), (70014, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_SPECIAL_OTHER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_SPECIAL_OTHER,
				"itemList": [
					(70008, 0), (72002, 0), (72005, 0), (72006, 0), (70042, 0), (70038, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_SPECIAL_DRAGON_VOUCHER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_SPECIAL_VOUCHER,
				"itemList": [
					(80017, 0), (80014, 0), (80018, 0), (80015, 0), (80016, 0), (72199, 0)
				]
			},

			ikashop.SHOP_SEARCH_SUB_SPECIAL_QUEST: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_SPECIAL_QUEST,
				"itemList": [
					(50307, 0), (50308, 0), (50309, 0), (50310, 0),
				]
			},

			ikashop.SHOP_SEARCH_SUB_SPECIAL_LOOTBOX: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_SPECIAL_LOOTBOX,
				"itemList": [
					(50006, 0), (50007, 0), (50008, 0), (50009, 0), (50011, 0), (50012, 0), (50013, 0), (50037, 0),
					(50024, 0), (50025, 0),
					(50070, 0), (50071, 0), (50073, 0), (50076, 0), (50077, 0), (50078, 0), (50079, 0), (50081, 0),
					(50082, 0), (50097, 0),
					(50098, 0),
				]
			},
		},
	},

	ikashop.SHOP_SEARCH_CATEGORY_ARMOR: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_ARMOR,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_ARMOR_BODY: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_ARMOR1,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_ARMOR_SHIELD: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_ARMOR2,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_ARMOR_HEAD: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_ARMOR3,
				"itemList": [
				]
			},
		}
	},

	ikashop.SHOP_SEARCH_CATEGORY_WEAPON: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_WEAPON,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_WEAPON_ONEHAND: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_WEAPON1,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_WEAPON_TWOHAND: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_WEAPON2,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_WEAPON_DAGGER: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_WEAPON3,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_WEAPON_BOW: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_WEAPON4,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_WEAPON_BELL: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_WEAPON5,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_WEAPON_FAN: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_WEAPON6,
				"itemList": [
				]
			},
		}
	},

	ikashop.SHOP_SEARCH_CATEGORY_JEWELRY: {
		"name": localeInfo.SHOP_SEARCH_CATEGORY_JEWELRY,
		"sub": {
			ikashop.SHOP_SEARCH_SUB_JEWELRY_EAR: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_JEWELRY1,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_JEWELRY_NECK: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_JEWELRY2,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_JEWELRY_WRIST: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_JEWELRY3,
				"itemList": [
				]
			},
			ikashop.SHOP_SEARCH_SUB_JEWELRY_BOOTS: {
				"name": localeInfo.SHOP_SEARCH_SUB_CATEGORY_JEWELRY4,
				"itemList": [
				]
			},
		}
	},
}

SHOP_SEARCH_CATEGORY_SORT = [
	ikashop.SHOP_SEARCH_CATEGORY_BOOKS,
	ikashop.SHOP_SEARCH_CATEGORY_REFINE,
	ikashop.SHOP_SEARCH_CATEGORY_SOULSTONE,
	ikashop.SHOP_SEARCH_CATEGORY_HERBALISM,
	ikashop.SHOP_SEARCH_CATEGORY_FISHING,
	ikashop.SHOP_SEARCH_CATEGORY_MINING,
	ikashop.SHOP_SEARCH_CATEGORY_HORSE,
	ikashop.SHOP_SEARCH_CATEGORY_POLYMORPH,
	ikashop.SHOP_SEARCH_CATEGORY_SPECIAL,
	ikashop.SHOP_SEARCH_CATEGORY_ARMOR,
	ikashop.SHOP_SEARCH_CATEGORY_WEAPON,
	ikashop.SHOP_SEARCH_CATEGORY_JEWELRY,
]

def is_wearable_search(category):
	return category in (ikashop.SHOP_SEARCH_CATEGORY_ARMOR, ikashop.SHOP_SEARCH_CATEGORY_WEAPON, ikashop.SHOP_SEARCH_CATEGORY_JEWELRY)

def search_category(category, sub_category=-1, isSearchAttr=False, selectedItem=None):
	print category, sub_category
	constInfo.OFFLINESHOP_LAST_SEARCHED_CATEGORY = category
	constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY = sub_category
	constInfo.OFFLINESHOP_LAST_SEARCH_IS_ATTR = isSearchAttr

	constInfo.OFFLINESHOP_LAST_SEARCHED_ITEMS = []

	searchCategory = category
	if is_wearable_search(category) and isSearchAttr:
		searchCategory += 1
	searchIndex = searchCategory * ikashop.SHOP_CATEGORY_MAX_SUB
	search_data = SHOP_SEARCH_FILTERS[category]
	if sub_category >= 0:
		search_data = search_data["sub"][sub_category]
		searchIndex += sub_category

	# shop search: one item picked in the grid -> search only that item.
	# The server reads it from the second argument as vnum * 1000 + socket0
	# (0 = whole category).
	searchItemCode = 0
	if selectedItem and selectedItem[1] >= 0 and selectedItem[1] < 1000:
		searchItemCode = selectedItem[0] * 1000 + selectedItem[1]
		constInfo.OFFLINESHOP_LAST_SEARCHED_ITEMS.append(selectedItem)
	else:
		for data in search_data["itemList"]:
			constInfo.OFFLINESHOP_LAST_SEARCHED_ITEMS.append(data)

	print searchIndex, searchItemCode
	ikashop.SendSearchItem(searchIndex, searchItemCode)

def get_item_list(category, sub_category=-1):
	data = SHOP_SEARCH_FILTERS[category]
	if data.has_key("sub"):
		return data["sub"][sub_category]["itemList"]
	else:
		return data["itemList"]

EVENT_OPEN_SHOP_SEARCH = "EVENT_OPEN_SHOP_SEARCH" # args | None
class ShopSearchWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()
		self.LoadDialog()

		eventManager.EventManager().add_observer(EVENT_OPEN_SHOP_SEARCH, self.Open)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def SetToolTip(self, itemToolTip):
		self.itemToolTip = proxy(itemToolTip)

	def __Initialize(self):
		self.categoryButtons = []
		self.subCategoryButtons = {}
		self.category = -1
		self.subCategory = -1
		self.itemToolTip = None
		self.isSearchAttr = False
		self.selectedItemIndex = -1

	@ui.WindowDestroy
	def Destroy(self):
		self.__Initialize()
		self.ClearDictionary()

	def LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/shopsearchwindow.py")

			self.titleBar = self.GetChild("TitleBar")

			self.categoryMask = self.GetChild("CategoryMask")
			self.categoryContent = self.GetChild("CategoryContent")
			self.scrollbar = self.GetChild("Scrollbar")

			self.subCategoryMask = self.GetChild("SubCategoryMask")
			self.subCategoryContent = self.GetChild("SubCategoryContent")
			self.subScrollbar = self.GetChild("SubScrollbar")

			self.itemSlot = self.GetChild("ItemSlot")
			self.searchButton = self.GetChild("SearchButton")
			self.clearButton = self.GetChild("ClearButton")
			self.infoLabel = self.GetChild("InfoLabel")
			self.searchAttrToggle = self.GetChild("SearchAttrToggle")

			self.itemSlot.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)
			self.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.__ShowToolTip))
			self.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.__HideToolTip))
			self.itemSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.__OnSelectItem))
			self.itemSlot.SetUnselectItemSlotEvent(ui.__mem_func__(self.__OnSelectItem))

			self.searchButton.SAFE_SetEvent(self.__OnSearch)
			self.clearButton.SAFE_SetEvent(self.__OnClear)

			self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

			self.scrollbar.SetScrollContent(self.categoryMask, self.categoryContent)
			self.scrollbar.SAFE_SetOnWheelEvent(self.categoryContent)

			self.subScrollbar.SetScrollContent(self.subCategoryMask, self.subCategoryContent)
			self.subScrollbar.SAFE_SetOnWheelEvent(self.subCategoryContent)

			for category in SHOP_SEARCH_CATEGORY_SORT:
				self.__CreateCategory(category)

			self.RefreshCategoryContentHeight(self.categoryButtons, self.categoryContent, self.scrollbar)
			self.subScrollbar.Hide()
			self.__OnClickCategory(-1)

			self.searchAttrToggle.SetToggleDownEvent(ui.__mem_func__(self.__OnToggleAttrSearch), True)
			self.searchAttrToggle.SetToggleUpEvent(ui.__mem_func__(self.__OnToggleAttrSearch), False)

		except:
			import exception
			exception.Abort("ShopSearchWindow.LoadDialog.BindObject")

	def __OnToggleAttrSearch(self, isSearch):
		self.isSearchAttr = isSearch
		print isSearch

	def __RefreshCategory(self):
		if self.category < 0:
			self.itemSlot.Hide()
			self.searchAttrToggle.Hide()
			self.infoLabel.Show()
			self.infoLabel.SetText(localeInfo.SHOP_SEARCH_NO_CATEGORY)

			self.searchButton.Down()
			self.searchButton.Disable()
			return

		self.searchButton.SetUp()
		self.searchButton.Enable()
		self.infoLabel.Hide()
		self.itemSlot.Hide()
		self.searchAttrToggle.Hide()
		self.itemSlot.Show()


		hasSubCategories = SHOP_SEARCH_FILTERS[self.category].has_key("sub")
		if not hasSubCategories or hasSubCategories and self.subCategory >= 0:
			self.searchButton.SetUp()
			self.searchButton.Enable()
			self.itemSlot.Show()
			self.infoLabel.Hide()

			self.__RefreshCategoryItems()
		else:
			self.itemSlot.Hide()
			self.infoLabel.Show()
			self.infoLabel.SetText(localeInfo.SHOP_SEARCH_NO_CATEGORY)
			self.searchButton.Down()
			self.searchButton.Disable()

		if is_wearable_search(self.category):
			if self.subCategory >= 0:
				self.searchAttrToggle.Show()
			self.itemSlot.Hide()


	def __RefreshCategoryItems(self):
		for y in range(self.itemSlot.y_height):
			for x in range(self.itemSlot.x_width):
				index = y * self.itemSlot.x_width + x
				self.itemSlot.SetItemSlot(index, 0, 0)
				self.itemSlot.ClearSlot(index)
				self.itemSlot.DeactivateSlot(index)

		itemList = get_item_list(self.category, self.subCategory)
		idx = 0
		for itemData in itemList:
			self.itemSlot.SetItemSlot(idx, itemData[0], 0, socket=(itemData[1], 0, 0))
			idx += 1

		if self.selectedItemIndex >= len(itemList):
			self.selectedItemIndex = -1
		if self.selectedItemIndex >= 0:
			self.itemSlot.ActivateSlot(self.selectedItemIndex)
		self.itemSlot.RefreshSlot()

	def __OnSelectItem(self, index):
		# click an item = search only that item, click it again = whole category
		if self.category < 0:
			return
		itemList = get_item_list(self.category, self.subCategory)
		if index < 0 or index >= len(itemList):
			return
		if self.selectedItemIndex == index:
			self.selectedItemIndex = -1
		else:
			self.selectedItemIndex = index
		self.__RefreshCategoryItems()

	def __GetSelectedItem(self):
		if self.selectedItemIndex < 0 or self.category < 0 or is_wearable_search(self.category):
			return None
		itemList = get_item_list(self.category, self.subCategory)
		if self.selectedItemIndex >= len(itemList):
			return None
		return itemList[self.selectedItemIndex]

	def __CreateCategory(self, categoryIndex):
		if categoryIndex == "break":
			step = ui.Window()
			step.SetParent(self.categoryContent)
			step.SetSize(1, 8)
			step.Show()
			self.categoryButtons.append((step, categoryIndex))
		else:
			data = SHOP_SEARCH_FILTERS[categoryIndex]
			categoryBtn = self.__CreateCategoryButton(self.categoryContent, data["name"])
			categoryBtn.SAFE_SetEvent(self.__OnClickCategory, categoryIndex)
			categoryBtn.SetClippingMaskWindow(self.categoryMask)
			self.categoryButtons.append((categoryBtn, categoryIndex))
			self.subCategoryButtons[categoryIndex] = []

			if data.has_key("sub"):
				subCategories = data["sub"]
				for i in range(len(subCategories)):
					if subCategories.has_key(i):
						subData = subCategories[i]
						subBtn = self.__CreateCategoryButton(self.subCategoryContent, subData["name"])
						subBtn.SetClippingMaskWindow(self.subCategoryMask)
						subBtn.SAFE_SetEvent(self.__OnClickSubCategory, i)
						self.subCategoryButtons[categoryIndex].append((subBtn, i))

	def __CreateCategoryButton(self, parent, name):
		btn = ui.Button()
		btn.SetParent(parent)
		btn.SetUpVisual("d:/ymir work/ui/flamewind/public/big_button_01.sub")
		btn.SetOverVisual("d:/ymir work/ui/flamewind/public/big_button_02.sub")
		btn.SetDownVisual("d:/ymir work/ui/flamewind/public/big_button_03.sub")
		btn.SetText(name, isBold=True)

		btn.ButtonText.SetHorizontalAlignLeft()
		btn.ButtonText.SetWindowVerticalAlignCenter()
		btn.ButtonText.SetPosition(7, 0)

		btn.SetWindowHorizontalAlignCenter()
		btn.Show()
		return btn

	def RefreshCategoryContentHeight(self, buttonList, contentWindow, scrollBar):
		start_y_pos = 10

		total_height = start_y_pos
		for data in buttonList:
			btn = data[0]
			btn.SetPosition(0, total_height)
			height = btn.GetHeight()
			height_offset = 5
			total_height += height + height_offset

		contentWindow.SetHeight(total_height)
		scrollBar.ResizeScrollBar()

	def __OnClickCategory(self, category):
		print "__OnClickCategory", category
		for data in self.categoryButtons:
			btn = data[0]
			if type(btn) is not ui.Button:
				continue

			if data[1] == category:
				btn.Down()
				btn.Disable()
			else:
				btn.SetUp()
				btn.Enable()

		self.category = category
		self.subCategory = -1
		self.selectedItemIndex = -1
		self.__RefreshSubCategoryButtons()
		self.__RefreshCategory()
		if category >= 0:
			self.RefreshCategoryContentHeight(self.subCategoryButtons[self.category], self.subCategoryContent, self.subScrollbar)

	def __RefreshSubCategoryButtons(self):
		for i, buttonList in self.subCategoryButtons.iteritems():
			for data in buttonList:
				btn = data[0]
				if i != self.category:
					btn.Hide()
					continue

				if type(btn) is not ui.Button:
					continue

				btn.Show()
				if data[1] == self.subCategory:
					btn.Down()
					btn.Disable()
				else:
					btn.SetUp()
					btn.Enable()

	def __OnClickSubCategory(self, index):
		self.subCategory = index
		self.selectedItemIndex = -1
		self.__RefreshSubCategoryButtons()
		self.__RefreshCategory()

	def __ShowToolTip(self, index):
		if self.itemToolTip:
			itemData = get_item_list(self.category, self.subCategory)[index]
			itemVnum = itemData[0]
			sockets = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)]
			sockets[0] = itemData[1]

			self.itemToolTip.SetRawItem(itemVnum, sockets)

	def __HideToolTip(self):
		if self.itemToolTip:
			self.itemToolTip.HideToolTip()

	def __OnSearch(self):
		if self.category >= 0:
			self.__OnClear()
			search_category(self.category, self.subCategory, self.isSearchAttr, self.__GetSelectedItem())

	def __OnClear(self):
		ikashop.ClearFoundShopMap()
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_SEARCH_CLEAR_INFO)
		uiPrivateShopBuilder.ClearViews()

	def Open(self):
		if self.IsShow():
			self.Close()
			return

		self.Show()
		self.SetTop()
		self.SetCenterPosition()

	def Close(self):
		self.Hide()
		self.__HideToolTip()

	def OnPressEscapeKey(self):
		self.Close()
		return True
