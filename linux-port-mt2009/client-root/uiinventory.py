import ui
import player
import mouseModule
import net
import app
import snd
import item
import player
import chat
import grp
import uiScriptLocale
import uiRefine
import uiAttachMetin
import uiPickMoney
import uiCommon
import uiPrivateShopBuilder
import localeInfo
import constInfo
import ime
import wndMgr
import dbg

import uiShop
import eventManager
import uiSafeBox
import uiExchange
import uiItemExchange
import safebox

import offlineShopManage
import uiPotionRecharge
import uiHorseInventory

if app.ENABLE_CHEQUE_SYSTEM:
	import uiToolTip
	import uiPickETC

if app.ENABLE_ACCE_COSTUME_SYSTEM:
	import acce
	
if app.ENABLE_IKASHOP_RENEWAL:
	import ikashop
	
from _weakref import proxy

EVENT_QUICK_ADD_INVENTORY_ITEM = "EVENT_QUICK_ADD_INVENTORY_ITEM" # args | destination: string, slotNumber: number
EVENT_CLOSE_INVENTORY = "EVENT_CLOSE_INVENTORY"

ITEM_MALL_BUTTON_ENABLE = True

ITEM_FLAG_APPLICABLE = 1 << 14

def GetLocalSlotAndInventoryPageFromGlobalSlot(globalSlot):
	if globalSlot >= player.INVENTORY_DEFAULT_MAX_NUM: # horse inventory
		return (globalSlot, 0)

	inventoryPage = int(globalSlot / player.INVENTORY_PAGE_SIZE)
	firstSlotOnPage = inventoryPage * player.INVENTORY_PAGE_SIZE
	localSlot = globalSlot - firstSlotOnPage
	return (localSlot, inventoryPage)

def GetGlobalSlotFromLocalSlotAndTab(localSlot, tab):
	if localSlot >= player.INVENTORY_DEFAULT_MAX_NUM: # horse inventory
		return localSlot

	return localSlot + player.INVENTORY_PAGE_SIZE * tab

class CostumeWindow(ui.ScriptWindow):

	def __init__(self, wndInventory):
		import exception

		if not app.ENABLE_COSTUME_SYSTEM:
			exception.Abort("What do you do?")
			return

		if not wndInventory:
			exception.Abort("wndInventory parameter must be set to InventoryWindow")
			return

		ui.ScriptWindow.__init__(self)

		self.isLoaded = 0
		self.wndInventory = wndInventory;

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()
		self.wndInventory = None

	def Show(self):
		self.__LoadWindow()
		self.RefreshCostumeSlot()

		ui.ScriptWindow.Show(self)

	def Close(self):
		self.Hide()

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/CostumeWindow.py")
		except:
			import exception
			exception.Abort("CostumeWindow.LoadWindow.LoadObject")

		try:
			wndEquip = self.GetChild("CostumeSlot")
			self.GetChild("TitleBar").SetCloseEvent(ui.__mem_func__(self.Close))

		except:
			import exception
			exception.Abort("CostumeWindow.LoadWindow.BindObject")

		## Equipment
		wndEquip.SetOverInItemEvent(ui.__mem_func__(self.wndInventory.OverInItem))
		wndEquip.SetOverOutItemEvent(ui.__mem_func__(self.wndInventory.OverOutItem))
		wndEquip.SetUnselectItemSlotEvent(ui.__mem_func__(self.wndInventory.UseItemSlot))
		wndEquip.SetUseSlotEvent(ui.__mem_func__(self.wndInventory.UseItemSlot))
		wndEquip.SetSelectEmptySlotEvent(ui.__mem_func__(self.wndInventory.SelectEmptySlot))
		wndEquip.SetSelectItemSlotEvent(ui.__mem_func__(self.wndInventory.SelectItemSlot))

		self.wndEquip = wndEquip

	def RefreshCostumeSlot(self):
		getItemVNum=player.GetItemIndex

		for i in xrange(item.COSTUME_SLOT_COUNT):
			slotNumber = item.COSTUME_SLOT_START + i
			self.wndEquip.SetItemSlot(slotNumber, getItemVNum(slotNumber), 0)

		if app.ENABLE_WEAPON_COSTUME_SYSTEM:
			self.wndEquip.SetItemSlot(item.COSTUME_SLOT_WEAPON, getItemVNum(item.COSTUME_SLOT_WEAPON), 0)

		self.wndEquip.RefreshSlot()

class BeltInventoryWindow(ui.ScriptWindow):

	def __init__(self, wndInventory):
		import exception

		if not app.ENABLE_NEW_EQUIPMENT_SYSTEM:
			exception.Abort("What do you do?")
			return

		if not wndInventory:
			exception.Abort("wndInventory parameter must be set to InventoryWindow")
			return

		ui.ScriptWindow.__init__(self)

		self.isLoaded = 0
		self.wndInventory = wndInventory

		self.wndBeltInventoryLayer = None
		self.wndBeltInventorySlot = None
		self.expandBtn = None
		self.minBtn = None

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	@ui.WindowDestroy
	def Destroy(self):
		self.wndInventory = None

	def Show(self, openBeltSlot = False):
		self.__LoadWindow()
		self.RefreshSlot()

		ui.ScriptWindow.Show(self)

		if openBeltSlot:
			self.OpenInventory()
		else:
			self.CloseInventory()

	def Close(self):
		self.Hide()

	def IsOpeningInventory(self):
		if self.wndBeltInventoryLayer:
			return self.wndBeltInventoryLayer.IsShow()
		return False

	def OpenInventory(self):
		self.wndBeltInventoryLayer.Show()
		self.expandBtn.Hide()

		if localeInfo.IsARABIC() == 0:
			self.AdjustPositionAndSize()

	def CloseInventory(self):
		self.wndBeltInventoryLayer.Hide()
		self.expandBtn.Show()

		if localeInfo.IsARABIC() == 0:
			self.AdjustPositionAndSize()

	def GetBasePosition(self):
		x, y = self.wndInventory.GetGlobalPosition()
		return x - 148, y + 241

	def AdjustPositionAndSize(self):
		bx, by = self.GetBasePosition()

		if self.IsOpeningInventory():
			self.SetPosition(bx, by)
			self.SetSize(self.ORIGINAL_WIDTH, self.GetHeight())

		else:
			self.SetPosition(bx + 138, by);
			self.SetSize(10, self.GetHeight())

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/BeltInventoryWindow.py")
		except:
			import exception
			exception.Abort("CostumeWindow.LoadWindow.LoadObject")

		try:
			self.ORIGINAL_WIDTH = self.GetWidth()
			wndBeltInventorySlot = self.GetChild("BeltInventorySlot")
			self.wndBeltInventoryLayer = self.GetChild("BeltInventoryLayer")
			self.expandBtn = self.GetChild("ExpandBtn")
			self.minBtn = self.GetChild("MinimizeBtn")

			self.expandBtn.SetEvent(ui.__mem_func__(self.OpenInventory))
			self.minBtn.SetEvent(ui.__mem_func__(self.CloseInventory))

			if localeInfo.IsARABIC() :
				self.expandBtn.SetPosition(self.expandBtn.GetWidth() - 2, 15)
				self.wndBeltInventoryLayer.SetPosition(self.wndBeltInventoryLayer.GetWidth() - 5, 0)
				self.minBtn.SetPosition(self.minBtn.GetWidth() + 3, 15)

			for i in xrange(item.BELT_INVENTORY_SLOT_COUNT):
				slotNumber = item.BELT_INVENTORY_SLOT_START + i
				wndBeltInventorySlot.SetCoverButton(slotNumber,	"d:/ymir work/ui/game/quest/slot_button_01.sub",\
												"d:/ymir work/ui/game/quest/slot_button_01.sub",\
												"d:/ymir work/ui/game/quest/slot_button_01.sub",\
												"d:/ymir work/ui/game/belt_inventory/slot_disabled.tga", False, False)

		except:
			import exception
			exception.Abort("CostumeWindow.LoadWindow.BindObject")

		## Equipment
		wndBeltInventorySlot.SetOverInItemEvent(ui.__mem_func__(self.wndInventory.OverInItem))
		wndBeltInventorySlot.SetOverOutItemEvent(ui.__mem_func__(self.wndInventory.OverOutItem))
		wndBeltInventorySlot.SetUnselectItemSlotEvent(ui.__mem_func__(self.wndInventory.UseItemSlot))
		wndBeltInventorySlot.SetUseSlotEvent(ui.__mem_func__(self.wndInventory.UseItemSlot))
		wndBeltInventorySlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.wndInventory.SelectEmptySlot))
		wndBeltInventorySlot.SetSelectItemSlotEvent(ui.__mem_func__(self.wndInventory.SelectItemSlot))

		self.wndBeltInventorySlot = wndBeltInventorySlot

	def RefreshSlot(self):
		getItemVNum=player.GetItemIndex

		for i in xrange(item.BELT_INVENTORY_SLOT_COUNT):
			slotNumber = item.BELT_INVENTORY_SLOT_START + i
			self.wndBeltInventorySlot.SetItemSlot(slotNumber, getItemVNum(slotNumber), player.GetItemCount(slotNumber))
			self.wndBeltInventorySlot.SetAlwaysRenderCoverButton(slotNumber, True)

			avail = "0"

			if player.IsAvailableBeltInventoryCell(slotNumber):
				self.wndBeltInventorySlot.EnableCoverButton(slotNumber)
			else:
				self.wndBeltInventorySlot.DisableCoverButton(slotNumber)

		self.wndBeltInventorySlot.RefreshSlot()

class GridSlotStateManager():
	SLOT_STATE_NONE = 0
	SLOT_STATE_IN_USE = 1
	SLOT_STATE_UNUSABLE = 2
	SLOT_STATE_TO_BE_SOLD = 3
	SLOT_STATE_NEW_ITEM = 4

	SLOT_MASK_COLOR_TO_BE_SOLD = (1, 1, 0, 0.3)

	def __init__(self, slotWindow, x, y):
		self.slotStates = [self.SLOT_STATE_NONE for i in range(x*y)]
		self.slotWindow = slotWindow
		self.secondSlotWindow = slotWindow

	def __del__(self):
		self.slotStates = []
		self.slotWindow = None

	def GetSlotWindow(self, globalSlot):
		if self.secondSlotWindow and globalSlot >= player.INVENTORY_DEFAULT_MAX_NUM:
			return self.secondSlotWindow
		return self.slotWindow

	def HideNonTradableItemSlots(self):
		pass

	def SetSlotState(self, slotIndex, state):
		if self.slotStates[slotIndex] != self.SLOT_STATE_NONE:
			self.MarkItemClean(slotIndex)

		self.slotStates[slotIndex] = state

	def MarkItemUnusable(self, slotIndex):
		self.SetSlotState(slotIndex, self.SLOT_STATE_UNUSABLE)
		self.GetSlotWindow(slotIndex).SetCantMouseEventSlot(slotIndex)

	def MarkItemInUse(self, slotIndex):
		self.SetSlotState(slotIndex, self.SLOT_STATE_IN_USE)
		self.GetSlotWindow(slotIndex).SetUnusableSlot(slotIndex)

	def MarkItemClean(self, globalSlotIndex):
		self.slotStates[globalSlotIndex] = self.SLOT_STATE_NONE

		(localSlot, page) = GetLocalSlotAndInventoryPageFromGlobalSlot(globalSlotIndex)
		self.GetSlotWindow(globalSlotIndex).SetCanMouseEventSlot(localSlot)
		self.GetSlotWindow(globalSlotIndex).SetUsableSlot(localSlot)
		self.GetSlotWindow(globalSlotIndex).DeactivateSlot(localSlot)

	def ClearSlotStates(self, *checkState):
		slotCount = len(self.slotStates)

		if len(checkState) > 0:
			for i in range(slotCount):
				if self.slotStates[i] in checkState:
					self.slotStates[i] = self.SLOT_STATE_NONE
		else:
			self.slotStates = [self.SLOT_STATE_NONE for i in range(slotCount)]
		self.RefreshAllSlots()

	def RefreshAllSlots(self):
		for i in range(len(self.slotStates)):
			self.RefreshSlotState(i, i)

	def RefreshSlotState(self, realSlotIndex, localSlotIndex=0):
		state = self.slotStates[realSlotIndex]

		if realSlotIndex >= player.INVENTORY_DEFAULT_MAX_NUM:
			localSlotIndex = realSlotIndex

		if state == self.SLOT_STATE_NONE:
			self.GetSlotWindow(realSlotIndex).SetCanMouseEventSlot(localSlotIndex)
			self.GetSlotWindow(realSlotIndex).SetUsableSlot(localSlotIndex)
		elif state == self.SLOT_STATE_UNUSABLE:
			self.GetSlotWindow(realSlotIndex).SetCantMouseEventSlot(localSlotIndex)
		elif state == self.SLOT_STATE_IN_USE:
			self.GetSlotWindow(realSlotIndex).SetUnusableSlot(localSlotIndex)
		elif state == self.SLOT_STATE_TO_BE_SOLD:
			self.GetSlotWindow(realSlotIndex).SetSlotMaskColor(localSlotIndex, self.SLOT_MASK_COLOR_TO_BE_SOLD)
		elif state == self.SLOT_STATE_NEW_ITEM:
			self.GetSlotWindow(realSlotIndex).ActivateSlot(localSlotIndex)

		self.GetSlotWindow(realSlotIndex).RefreshSlot()

	def OnSlotMouseOverIn(self, slot):
		pass

	def OnSlotMouseOverOut(self, slot):
		pass

class InventorySlotManager(GridSlotStateManager):
	def __init__(self, slotWindow):
		GridSlotStateManager.__init__(self, slotWindow, player.INVENTORY_PAGE_SIZE, player.INVENTORY_PAGE_COUNT)
		self.secondSlotWindow = None
		self.tab = 0

		self.isMyShopManageOpen = False

		eventManager.EventManager().add_observer(uiShop.EVENT_ADD_MASS_SELL, self.OnMassSellItemAdd)
		eventManager.EventManager().add_observer(uiShop.EVENT_STOP_MASS_SELL, self.OnMassSellStop)

		eventManager.EventManager().add_observer(uiAttachMetin.EVENT_ATTACH_METIN_OPEN, self.OnAttachMetin)
		eventManager.EventManager().add_observer(uiAttachMetin.EVENT_ATTACH_METIN_CLOSE, self.OnRefineClose)

		eventManager.EventManager().add_observer(uiRefine.EVENT_OPEN_REFINE, self.OnRefineItem)
		eventManager.EventManager().add_observer(uiRefine.EVENT_CLOSE_REFINE, self.OnRefineClose)

		eventManager.EventManager().add_observer(uiExchange.EVENT_OPEN_EXCHANGE, self.SetItemSlotLimit_Exchange)
		eventManager.EventManager().add_observer(uiExchange.EVENT_ADD_ITEM_TO_EXCHANGE, self.OnAddItemToExchange)
		eventManager.EventManager().add_observer(uiExchange.EVENT_CLOSE_EXCHANGE, self.OnExchangeClose)

		eventManager.EventManager().add_observer(uiItemExchange.EVENT_ADD_ITEM_TO_ITEM_EXCHANGE, self.OnAddItemToItemExchange)
		eventManager.EventManager().add_observer(uiItemExchange.EVENT_REMOVE_ITEM_EXCHANGE, self.OnRemoveItemFromItemExchange)
		eventManager.EventManager().add_observer(uiItemExchange.EVENT_ITEM_EXCHANGE_CLOSE, self.OnExchangeItemClose)

		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_ADD_AUTOPOTION_SLOT,self.OnAddItemToItemExchange)
		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_ADD_ITEM_SLOT,self.OnAddItemToItemExchange)
		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_REMOVE_ITEM,self.OnRemoveItemFromItemExchange)
		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_POTION_CLOSE, self.OnExchangeItemClose)

		eventManager.EventManager().add_observer(uiSafeBox.EVENT_OPEN_SAFEBOX, self.SetItemSlotLimit_Safebox)
		eventManager.EventManager().add_observer(uiSafeBox.EVENT_CLOSE_SAFEBOX, self.OnSafeboxClose)

		eventManager.EventManager().add_observer(offlineShopManage.EVENT_OPEN_MYSHOP_SHOP_MANAGE, self.SetItemSlotLimit_PlayerShop)
		eventManager.EventManager().add_observer(offlineShopManage.EVENT_CLOSE_MYSHOP_SHOP_MANAGE, self.__ClearNonTradableSlots)

		eventManager.EventManager().add_observer(uiShop.EVENT_OPEN_SHOP_DIALOG, self.SetItemSlotLimit_NPCShop)
		eventManager.EventManager().add_observer(uiShop.EVENT_CLOSE_SHOP_DIALOG, self.__ClearSlotsOnShopDialogClose)

		eventManager.EventManager().add_observer(eventManager.OPEN_PRIVATE_SHOP_BUILDER, self.SetItemSlotLimit_PlayerShop)
		eventManager.EventManager().add_observer(eventManager.CLOSE_PRIVATE_SHOP_BUILDER, self.OnExchangeClose)

		eventManager.EventManager().add_observer(eventManager.ADD_ITEM_TO_SHOP_BUILDER, self.OnAddItemToShopBuilder)
		eventManager.EventManager().add_observer(eventManager.REMOVE_ITEM_FROM_SHOP_BUILDER, self.OnRemoveItemFromShopBuilder)

	def __del__(self):
		GridSlotStateManager.__del__(self)

	def OnMassSellItemAdd(self, sourceSlotPos, sourceWindowType):
		(localSlot, page) = GetLocalSlotAndInventoryPageFromGlobalSlot(sourceSlotPos)
		if self.slotStates[sourceSlotPos] == self.SLOT_STATE_TO_BE_SOLD:
			self.slotStates[sourceSlotPos] = self.SLOT_STATE_NONE
			self.GetSlotWindow(sourceSlotPos).SetSlotMaskNormalColor(localSlot)
			return
		else:
			self.slotStates[sourceSlotPos] = self.SLOT_STATE_TO_BE_SOLD
			self.GetSlotWindow(sourceSlotPos).SetSlotMaskColor(localSlot, self.SLOT_MASK_COLOR_TO_BE_SOLD)

	def OnMassSellItemRemove(self, sourceSlotPos, sourceWindowType):
		(localSlot, page) = GetLocalSlotAndInventoryPageFromGlobalSlot(sourceSlotPos)

	def OnRefineClose(self):
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE)

	def OnSafeboxClose(self):
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE)

	def OnExchangeItemClose(self): # nie handel
		self.ClearSlotStates(self.SLOT_STATE_IN_USE)

	def OnExchangeClose(self): # handel
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE, self.SLOT_STATE_IN_USE)

	def OnMassSellStop(self):
		self.ClearSlotStates(self.SLOT_STATE_TO_BE_SOLD)

	def __ClearNonTradableSlots(self):
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE)

	def __ClearSlotsOnShopDialogClose(self):
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE, self.SLOT_STATE_TO_BE_SOLD)

	def OnAttachMetin(self, itemSlotPos, metinSlotPos):
		(localSlot1, page) = GetLocalSlotAndInventoryPageFromGlobalSlot(itemSlotPos)
		(localSlot2, page2) = GetLocalSlotAndInventoryPageFromGlobalSlot(metinSlotPos)

		self.slotStates[itemSlotPos] = self.SLOT_STATE_UNUSABLE
		self.slotStates[metinSlotPos] = self.SLOT_STATE_UNUSABLE

		if self.tab == page:
			self.GetSlotWindow(itemSlotPos).SetCantMouseEventSlot(localSlot1)

		if self.tab == page2:
			self.GetSlotWindow(metinSlotPos).SetCantMouseEventSlot(localSlot2)

	def OnRefineItem(self, itemSlotPos):
		self.MarkItemUnusable(itemSlotPos)

	def OnAddItemToItemExchange(self, sourceSlotPos): # nie handel
		self.MarkItemInUse(sourceSlotPos)

	def OnRemoveItemFromItemExchange(self, sourceSlotPos): # nie handel
		self.MarkItemClean(sourceSlotPos)

	def OnAddItemToExchange(self, sourceWindowType, sourceSlotPos): # handel
		self.MarkItemInUse(sourceSlotPos)

	def OnAddItemToShopBuilder(self, sourceSlotPos, sourceWindowType):
		self.MarkItemInUse(sourceSlotPos)

	def OnRemoveItemFromShopBuilder(self, sourceSlotPos, sourceWindowType):
		self.MarkItemClean(sourceSlotPos)

	def SetItemSlotLimit_NPCShop(self):
		self.SetItemSlotLimit(item.ITEM_ANTIFLAG_SELL)

	def SetItemSlotLimit_PlayerShop(self):
		self.SetItemSlotLimit(item.ITEM_ANTIFLAG_MYSHOP)

	def SetItemSlotLimit_Safebox(self):
		self.SetItemSlotLimit(item.ITEM_ANTIFLAG_SAFEBOX)

	def SetItemSlotLimit_Exchange(self):
		self.SetItemSlotLimit(item.ITEM_ANTIFLAG_GIVE)

	def SetItemSlotLimit(self, antiflag):
		for slot in range(player.INVENTORY_PAGE_SIZE * player.INVENTORY_PAGE_COUNT):
			isAntiflag = player.IsAntiFlagBySlot(slot, antiflag)
			if isAntiflag:
				self.SetSlotState(slot, self.SLOT_STATE_UNUSABLE)

		self.RefreshAllSlots()

	def SetTab(self, tab):
		self.tab = tab

	def MarkItemUnusable(self, globalSlotIndex):
		self.SetSlotState(globalSlotIndex, self.SLOT_STATE_UNUSABLE)

		localSlot, page = GetLocalSlotAndInventoryPageFromGlobalSlot(globalSlotIndex)
		self.GetSlotWindow(globalSlotIndex).SetCantMouseEventSlot(localSlot)

	def MarkItemInUse(self, globalSlotIndex):
		self.SetSlotState(globalSlotIndex, self.SLOT_STATE_IN_USE)

		localSlot, page = GetLocalSlotAndInventoryPageFromGlobalSlot(globalSlotIndex)
		self.GetSlotWindow(globalSlotIndex).SetUnusableSlot(localSlot)

	def MarkItemClean(self, slotIndex):
		self.slotStates[slotIndex] = self.SLOT_STATE_NONE
		self.GetSlotWindow(slotIndex).SetCanMouseEventSlot(slotIndex)
		self.GetSlotWindow(slotIndex).SetUsableSlot(slotIndex)
		self.GetSlotWindow(slotIndex).DeactivateSlot(slotIndex)

	def RefreshAllSlots(self):
		for i in range(player.INVENTORY_PAGE_SIZE):
			realSlot = player.INVENTORY_PAGE_SIZE * self.tab + i
			self.RefreshSlotState(realSlot, i)

		for i in range(player.INVENTORY_PAGE_SIZE):
			realSlot = player.INVENTORY_DEFAULT_MAX_NUM + i
			self.RefreshSlotState(realSlot, i)

	def HighlightSlot(self, inventorySlot):
		if inventorySlot < len(self.slotStates):
			self.slotStates[inventorySlot] = self.SLOT_STATE_NEW_ITEM

	def OnSlotMouseOverIn(self, slot):
		if slot >= len(self.slotStates):
			return

		globalSlot = GetGlobalSlotFromLocalSlotAndTab(slot, self.tab)
		slotState = self.slotStates[globalSlot]
		if slotState == self.SLOT_STATE_NEW_ITEM:
			self.slotStates[globalSlot] = self.SLOT_STATE_NONE
			self.GetSlotWindow(globalSlot).DeactivateSlot(slot)

class InventoryWindow(ui.ScriptWindow):

	USE_TYPE_TUPLE = ("USE_CLEAN_SOCKET", "USE_CHANGE_ATTRIBUTE", "USE_ADD_ATTRIBUTE", "USE_ADD_ATTRIBUTE2", "USE_ADD_ACCESSORY_SOCKET", "USE_PUT_INTO_ACCESSORY_SOCKET", "USE_PUT_INTO_BELT_SOCKET", "USE_PUT_INTO_RING_SOCKET")
	if app.ENABLE_USE_COSTUME_ATTR:
		USE_TYPE_TUPLE = tuple(list(USE_TYPE_TUPLE) + ["USE_CHANGE_COSTUME_ATTR", "USE_RESET_COSTUME_ATTR"])

	questionDialog = None
	tooltipItem = None
	wndCostume = None
	wndBelt = None
	dlgPickMoney = None
	if app.ENABLE_CHEQUE_SYSTEM:
		dlgPickETC = None

	sellingSlotNumber = -1
	isLoaded = 0
	isOpenedCostumeWindowWhenClosingInventory = 0
	isOpenedBeltWindowWhenClosingInventory = 0

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.isOpenedBeltWindowWhenClosingInventory = 0

		self.inventoryPageIndex = 0

		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			self.wndAcceCombine = None
			self.wndAcceAbsorption = None

		self.isExchangeDialogOpen = False
		self.isOfflineShopBuilderOpen = False
		self.isOfflineShopManageOpen = False
		self.isSafeboxOpen = False
		self.isExchangeItemOpen = False
		self.isRechargePotion = False
		self.wndHorseInventory = None
		eventManager.EventManager().add_observer(uiExchange.EVENT_OPEN_EXCHANGE, self.OnExchangeDialogOpen) # handel
		eventManager.EventManager().add_observer(uiExchange.EVENT_CLOSE_EXCHANGE, self.OnExchangeDialogClose) # handel

		eventManager.EventManager().add_observer(eventManager.OPEN_PRIVATE_SHOP_BUILDER, self.OnOfflineShopBuilderOpen)
		eventManager.EventManager().add_observer(eventManager.CLOSE_PRIVATE_SHOP_BUILDER, self.OnOfflineShopBuilderClose)

		eventManager.EventManager().add_observer(offlineShopManage.EVENT_OPEN_MYSHOP_SHOP_MANAGE, self.OnOfflineShopManageOpen)
		eventManager.EventManager().add_observer(offlineShopManage.EVENT_CLOSE_MYSHOP_SHOP_MANAGE, self.OnOfflineShopManageClose)

		eventManager.EventManager().add_observer(uiSafeBox.EVENT_OPEN_SAFEBOX, self.OnSafeboxOpen)
		eventManager.EventManager().add_observer(uiSafeBox.EVENT_CLOSE_SAFEBOX, self.OnSafeboxClose)

		eventManager.EventManager().add_observer(uiItemExchange.EVENT_ITEM_EXCHANGE_OPEN, self.OnExchangeItemOpen) # nie handel
		eventManager.EventManager().add_observer(uiItemExchange.EVENT_ITEM_EXCHANGE_CLOSE, self.OnExchangeItemClose) # nie handel

		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_POTION_OPEN, self.OnRechargePotionOpen)
		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_POTION_CLOSE, self.OnRechargePotionClose)

		eventManager.EventManager().add_observer(uiSafeBox.EVENT_QUICK_REMOVE_SAFEBOX_ITEM, self.OnQuickRemoveSafeboxItem)

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Show(self):
		self.__LoadWindow()

		ui.ScriptWindow.Show(self)

		if self.isOpenedCostumeWindowWhenClosingInventory and self.wndCostume:
			self.wndCostume.Show()

		if self.wndBelt:
			self.wndBelt.Show(self.isOpenedBeltWindowWhenClosingInventory)

	def BindInterfaceClass(self, interface):
		self.interface = interface
		if app.ENABLE_WON_EXCHANGE_WINDOW and self.interface and self.wndChequeSlot:
			self.wndChequeSlot.SetEvent(ui.__mem_func__(self.interface.ToggleWonExchangeWindow))

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/InventoryWindow.py")
		except:
			import exception
			exception.Abort("InventoryWindow.LoadWindow.LoadObject")

		try:
			wndItem = self.GetChild("ItemSlot")
			wndEquip = self.GetChild("EquipmentSlot")
			self.GetChild("TitleBar").SetCloseEvent(ui.__mem_func__(self.Close))
			self.wndMoney = self.GetChild("Money")
			self.wndMoneySlot = self.GetChild("Money_Slot")
			self.DSSButton = self.GetChild2("DSSButton")
			self.costumeButton = self.GetChild2("CostumeButton")

			# additional buttons
			self.horseInventoryButton = self.GetChild("HorseInventoryWindow")
			self.depositButton = self.GetChild("DepositButton")
			self.myshopButton = self.GetChild("MyShopButton")

			if app.ENABLE_CHEQUE_SYSTEM:
				self.wndCheque = self.GetChild("Cheque")
				self.wndChequeSlot = self.GetChild("Cheque_Slot")

				self.wndMoneyIcon = self.GetChild("Money_Icon")
				self.wndChequeIcon = self.GetChild("Cheque_Icon")

				self.wndMoneyIcon.SetEvent(ui.__mem_func__(self.EventProgress), "mouse_over_in", 0)
				self.wndChequeIcon.SetEvent(ui.__mem_func__(self.EventProgress), "mouse_over_in", 1)

				self.wndMoneyIcon.SetEvent(ui.__mem_func__(self.EventProgress), "mouse_over_out", 0)
				self.wndChequeIcon.SetEvent(ui.__mem_func__(self.EventProgress), "mouse_over_out", 1)

				self.toolTip = uiToolTip.ToolTip()
				self.toolTip.ClearToolTip()

			self.autostackButton = self.GetChild("AutoStackButton")
			self.autostackButton.SAFE_SetEvent(self.__OnAutoStackButton)


			self.inventoryTab = []
			for i in xrange(player.INVENTORY_PAGE_COUNT-1):
				self.inventoryTab.append(self.GetChild("Inventory_Tab_%02d" % (i+1)))

			self.equipmentTab = []
			self.equipmentTab.append(self.GetChild("Equipment_Tab_01"))
			self.equipmentTab.append(self.GetChild("Equipment_Tab_02"))

			if self.costumeButton and not app.ENABLE_COSTUME_SYSTEM:
				self.costumeButton.Hide()
				self.costumeButton.Destroy()
				self.costumeButton = 0

			# Belt Inventory Window
			self.wndBelt = None

			if app.ENABLE_NEW_EQUIPMENT_SYSTEM:
				self.wndBelt = BeltInventoryWindow(self)

		except:
			import exception
			exception.Abort("InventoryWindow.LoadWindow.BindObject")

		## Item
		wndItem.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		wndItem.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		wndItem.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndItem.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndItem.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		wndItem.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))

		## Equipment
		wndEquip.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		wndEquip.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		wndEquip.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndEquip.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndEquip.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		wndEquip.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))

		## PickMoneyDialog
		dlgPickMoney = uiPickMoney.PickMoneyDialog()
		dlgPickMoney.LoadDialog()
		dlgPickMoney.Hide()

		## PickETCDialog
		if app.ENABLE_CHEQUE_SYSTEM:
			dlgPickETC = uiPickETC.PickETCDialog()
			dlgPickETC.LoadDialog()
			dlgPickETC.Hide()

		## RefineDialog
		self.refineDialog = uiRefine.RefineDialog()
		self.refineDialog.Hide()

		## AttachMetinDialog
		self.attachMetinDialog = uiAttachMetin.AttachMetinDialog()
		self.attachMetinDialog.Hide()

		## MoneySlot
		if app.ENABLE_CHEQUE_SYSTEM:
			self.wndChequeSlot.SetEvent(ui.__mem_func__(self.OpenPickMoneyDialog), 1)
			self.wndMoneySlot.SetEvent(ui.__mem_func__(self.OpenPickMoneyDialog), 0)
		else:
			self.wndMoneySlot.SetEvent(ui.__mem_func__(self.OpenPickMoneyDialog))

		for i in xrange(player.INVENTORY_PAGE_COUNT-1):
			self.inventoryTab[i].SAFE_SetEvent(self.SetInventoryPage, i)
		self.inventoryTab[0].Down()

		self.equipmentTab[0].SAFE_SetEvent(self.SetEquipmentPage, 0)
		self.equipmentTab[1].SAFE_SetEvent(self.SetEquipmentPage, 1)
		self.equipmentTab[0].Down()
		self.equipmentTab[0].Hide()
		self.equipmentTab[1].Hide()

		self.wndItem = wndItem
		self.wndEquip = wndEquip
		self.dlgPickMoney = dlgPickMoney

		# Additional buttons
		if self.horseInventoryButton:
			self.horseInventoryButton.SAFE_SetEvent(self.ClickHorseInventoryButton)

		if self.depositButton:
			self.depositButton.SetEvent(ui.__mem_func__(self.ClickDepositButton))

		if self.myshopButton:
			self.myshopButton.SetEvent(ui.__mem_func__(self.ClickMyShopButton))

		self.inventorySlotStateMgr = InventorySlotManager(self.wndItem)

		if app.ENABLE_CHEQUE_SYSTEM:
			self.dlgPickETC = dlgPickETC

		if self.DSSButton:
			self.DSSButton.SetEvent(ui.__mem_func__(self.ClickDSSButton))

		# Costume Button
		if self.costumeButton:
			self.costumeButton.SetEvent(ui.__mem_func__(self.ClickCostumeButton))

		self.wndCostume = None

 		#####
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			self.listAttachedAcces = []

		## Refresh
		self.SetInventoryPage(0)
		self.SetEquipmentPage(0)
		self.RefreshItemSlot()
		self.RefreshGold()

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()

		if self.dlgPickMoney:
			self.dlgPickMoney.Destroy()
			self.dlgPickMoney = 0

		if self.refineDialog:
			self.refineDialog.Destroy()
			self.refineDialog = 0

		if app.ENABLE_CHEQUE_SYSTEM and self.dlgPickETC:
			self.dlgPickETC.Destroy()
			self.dlgPickETC = 0

		if self.attachMetinDialog:
			self.attachMetinDialog.Destroy()
			self.attachMetinDialog = 0

		self.tooltipItem = None
		self.wndItem = 0
		self.wndEquip = 0
		self.dlgPickMoney = 0
		self.wndMoney = 0
		self.wndMoneySlot = 0
		if app.ENABLE_CHEQUE_SYSTEM:
			self.wndCheque = 0
			self.wndChequeSlot = 0
			self.dlgPickETC = 0
		self.questionDialog = None
		self.mallButton = None
		self.DSSButton = None
		self.costumeButton = None
		self.wndDragonSoulRefine = None
		self.interface = None

		self.wndHorseInventory = None

		if self.wndCostume:
			self.wndCostume.Destroy()
			self.wndCostume = None

		if self.wndBelt:
			self.wndBelt.Destroy()
			self.wndBelt = None

		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			self.wndAcceCombine = None
			self.wndAcceAbsorption = None

		self.inventoryTab = []
		self.equipmentTab = []

		self.inventorySlotStateMgr = None

	def Hide(self):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS():
			self.OnCloseQuestionDialog()
			return
		if None != self.tooltipItem:
			self.tooltipItem.HideToolTip()

		if self.wndCostume:
			self.isOpenedCostumeWindowWhenClosingInventory = self.wndCostume.IsShow()
			self.wndCostume.Close()

		if self.wndBelt:
			self.isOpenedBeltWindowWhenClosingInventory = self.wndBelt.IsOpeningInventory()
			print "Is Opening Belt Inven?? ", self.isOpenedBeltWindowWhenClosingInventory
			self.wndBelt.Close()

		if self.dlgPickMoney:
			self.dlgPickMoney.Close()

		if app.ENABLE_CHEQUE_SYSTEM:
			if self.dlgPickETC:
				self.dlgPickETC.Close()

		wndMgr.Hide(self.hWnd)


	def Close(self):
		eventManager.EventManager().send_event(EVENT_CLOSE_INVENTORY)
		self.Hide()

	def SetHorseInventory(self, wndHorseInventory):
		self.wndHorseInventory = proxy(wndHorseInventory)

		self.wndHorseInventory.itemSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		self.wndHorseInventory.itemSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		self.wndHorseInventory.itemSlot.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		self.wndHorseInventory.itemSlot.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		self.wndHorseInventory.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.wndHorseInventory.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))

		self.inventorySlotStateMgr.secondSlotWindow = self.wndHorseInventory.itemSlot

	def SetInventoryPage(self, page):
		self.inventoryPageIndex = page
		for i in xrange(player.INVENTORY_PAGE_COUNT-1):
			if i!=page:
				self.inventoryTab[i].SetUp()

		self.inventorySlotStateMgr.SetTab(page)
		self.RefreshBagSlotWindow()

	def ClickHorseInventoryButton(self):
		eventManager.EventManager().send_event(uiHorseInventory.EVENT_OPEN_HORSE_INVENTORY)

	def ClickDepositButton(self):
		print "ClickDepositButton"
		net.SendChatPacket("/click_mall")

	def ClickMyShopButton(self):
		print "ClickMyShopButton"
		self.interface.OpenPrivateShopManage()

	def SetEquipmentPage(self, page):
		self.equipmentPageIndex = page
		self.equipmentTab[1-page].SetUp()
		self.RefreshEquipSlotWindow()

	# DSSButton
	def ClickDSSButton(self):
		print "click_dss_button"
		self.interface.ToggleDragonSoulWindow()

	def ClickCostumeButton(self):
		print "Click Costume Button"
		if self.wndCostume:
			if self.wndCostume.IsShow():
				self.wndCostume.Hide()
			else:
				self.wndCostume.Show()
		else:
			self.wndCostume = CostumeWindow(self)
			self.wndCostume.Show()


	def __OnAutoStackButton(self):
		import inventoryarrange
		inventoryarrange.Request()

	def __OnAutoStackButtonByMoves(self):
		import autostackpump
		moves = []
		TOTAL_SLOTS = player.INVENTORY_MAX_NUM
		for sourceSlot in range(TOTAL_SLOTS):
			srcItemVnum = player.GetItemIndex(sourceSlot)

			if srcItemVnum == 0:
				continue

			item.SelectItem(srcItemVnum)
			if item.IsFlag(item.ITEM_FLAG_STACKABLE):
				for destSlot in range(sourceSlot + 1, TOTAL_SLOTS):
					destItemVnum = player.GetItemIndex(destSlot)

					if destItemVnum == srcItemVnum:
						moves.append((destSlot, sourceSlot))

		autostackpump.Queue(moves)
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.AUTOSTACK_INVENTORY)

	def OpenPickMoneyDialog(self, focus_idx = 0):
		pass
		# if mouseModule.mouseController.isAttached():
		# 	attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
		# 	if player.SLOT_TYPE_SAFEBOX == mouseModule.mouseController.GetAttachedType():
		# 		if player.ITEM_MONEY == mouseModule.mouseController.GetAttachedItemIndex():
		# 			net.SendSafeboxWithdrawMoneyPacket(mouseModule.mouseController.GetAttachedItemCount())
		# 			snd.PlaySound("sound/ui/money.wav")
		#
		# 	mouseModule.mouseController.DeattachObject()
		#
		# else:
		# 	curMoney = player.GetElk()
		# 	if app.ENABLE_CHEQUE_SYSTEM:
		# 		curCheque = player.GetCheque()
		# 		if curMoney <= 0 and curCheque <= 0:
		# 			return
		# 	else:
		# 		curCheque = 1 # default money value
		# 		if curMoney <= 0:
		# 			return
		#
		# 	self.dlgPickMoney.SetTitleName(localeInfo.PICK_MONEY_TITLE)
		# 	self.dlgPickMoney.SetAcceptEvent(ui.__mem_func__(self.OnPickMoney))
		# 	self.dlgPickMoney.Open(curMoney, curCheque)
		# 	if app.ENABLE_CHEQUE_SYSTEM:
		# 		if focus_idx==0:
		# 			self.dlgPickMoney.SetMaxCheque(3)
		# 			self.dlgPickMoney.SetMax(9)
		# 		else:
		# 			self.dlgPickMoney.SetMax(9)
		# 			self.dlgPickMoney.SetMaxCheque(3)
		# 		self.dlgPickMoney.SetFocus(focus_idx)
		# 	else:
		# 		self.dlgPickMoney.SetMax(9)

	def OnPickMoney(self, money, cheque=0):
		mouseModule.mouseController.AttachMoney(self, player.SLOT_TYPE_INVENTORY, money, cheque)

	def OnPickItem(self, count):
		if app.ENABLE_CHEQUE_SYSTEM:
			itemSlotIndex = self.dlgPickETC.itemGlobalSlotIndex
		else:
			itemSlotIndex = self.dlgPickMoney.itemGlobalSlotIndex
		selectedItemVNum = player.GetItemIndex(itemSlotIndex)
		mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_INVENTORY, itemSlotIndex, selectedItemVNum, count)

	def __InventoryLocalSlotPosToGlobalSlotPos(self, local):
		if local >= player.INVENTORY_DEFAULT_MAX_NUM:
			return local

		if player.IsEquipmentSlot(local) or player.IsCostumeSlot(local) or (app.ENABLE_NEW_EQUIPMENT_SYSTEM and player.IsBeltInventorySlot(local)):
			return local

		return self.inventoryPageIndex*player.INVENTORY_PAGE_SIZE + local

	def RefreshBagSlotWindow(self, wndSlot=None):
		getItemVNum=player.GetItemIndex
		getItemCount=player.GetItemCount

		if wndSlot is None:
			wndSlot = self.wndItem

		setItemVNum=wndSlot.SetItemSlot

		for i in xrange(player.INVENTORY_PAGE_SIZE):
			localSlot = i + wndSlot.GetStartIndex()
			slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(localSlot)

			itemCount = getItemCount(slotNumber)
			if 0 == itemCount:
				wndSlot.ClearSlot(localSlot)
				continue
			elif 1 == itemCount:
				itemCount = 0

			itemVnum = getItemVNum(slotNumber)

			socket = tuple(player.GetItemMetinSocket(slotNumber, j) for j in range(player.METIN_SOCKET_MAX_NUM))

			setItemVNum(localSlot, itemVnum, itemCount, socket=socket)
			# if app.ENABLE_IKASHOP_RENEWAL:
			# 	if app.EXTEND_IKASHOP_PRO:
			# 		self.wndItem.EnableSlot(i)
			# 		board = ikashop.GetBusinessBoard()
			# 		if board and (board.IsShow() or board.IsCreatingAuction()):
			# 			item.SelectItem(itemVnum)
			# 			if item.IsAntiFlag(item.ANTIFLAG_GIVE) or item.IsAntiFlag(item.ANTIFLAG_MYSHOP):
			# 				self.wndItem.DisableSlot(i)




			if constInfo.IS_AUTO_POTION(itemVnum):
				metinSocket = [player.GetItemMetinSocket(slotNumber, j) for j in xrange(player.METIN_SOCKET_MAX_NUM)]

				isActivated = 0 != metinSocket[0]

				if isActivated:
					wndSlot.ActivateSlot(i)
					potionType = 0
					if constInfo.IS_AUTO_POTION_HP(itemVnum):
						potionType = player.AUTO_POTION_TYPE_HP
					elif constInfo.IS_AUTO_POTION_SP(itemVnum):
						potionType = player.AUTO_POTION_TYPE_SP

					usedAmount = int(metinSocket[1])
					totalAmount = int(metinSocket[2])
					player.SetAutoPotionInfo(potionType, isActivated, (totalAmount - usedAmount), totalAmount, self.__InventoryLocalSlotPosToGlobalSlotPos(i))

				else:
					wndSlot.DeactivateSlot(i)

			if constInfo.ENABLE_ACTIVE_PET_SEAL_EFFECT and constInfo.IS_PET_SEAL(itemVnum):
				metinSocket = [player.GetItemMetinSocket(slotNumber, j) for j in xrange(player.METIN_SOCKET_MAX_NUM)]
				isActivated = 0 != metinSocket[2]
				if isActivated:
					wndSlot.ActivateSlot(i)
				else:
					wndSlot.DeactivateSlot(i)

		wndSlot.RefreshSlot()

		if self.wndBelt:
			self.wndBelt.RefreshSlot()

		self.inventorySlotStateMgr.RefreshAllSlots()

	def HighlightSlot(self, inventorySlot):
		self.inventorySlotStateMgr.HighlightSlot(inventorySlot)

	def OnExchangeDialogOpen(self): # handel
		self.isExchangeDialogOpen = True

	def OnExchangeDialogClose(self): # handel
		self.isExchangeDialogOpen = False

	def OnOfflineShopBuilderOpen(self):
		self.isOfflineShopBuilderOpen = True

	def OnOfflineShopBuilderClose(self):
		self.isOfflineShopBuilderOpen = False

	def OnOfflineShopManageOpen(self):
		self.isOfflineShopManageOpen = True

	def OnOfflineShopManageClose(self):
		self.isOfflineShopManageOpen = False

	def OnSafeboxOpen(self):
		self.isSafeboxOpen = True

	def OnSafeboxClose(self):
		self.isSafeboxOpen = False

	def OnExchangeItemOpen(self): # nie handel
		self.isExchangeItemOpen = True

	def OnExchangeItemClose(self): # nie handel
		self.isExchangeItemOpen = False

	def OnRechargePotionOpen(self):
		self.isRechargePotion = True

	def OnRechargePotionClose(self):
		self.isRechargePotion = False

	def OnQuickRemoveSafeboxItem(self, type, slotNumber):
		itemVnum = 0
		if type == "safebox":
			itemVnum = safebox.GetItemID(slotNumber)
		elif type == "mall":
			itemVnum = safebox.GetMallItemID(slotNumber)

		if itemVnum > 0:
			item.SelectItem(itemVnum)
			(itemWidth, itemHeight) = item.GetItemSize()

			freeSlot = self.wndItem.FindEmptySlot(itemHeight, self.inventoryPageIndex * player.INVENTORY_PAGE_SIZE)
			if freeSlot >= 0:
				if type == "safebox":
					net.SendSafeboxCheckoutPacket(slotNumber, freeSlot)
				elif type == "mall":
					net.SendMallCheckoutPacket(slotNumber, freeSlot)

	def SetItemSlotVnum(self, slotNumber):
		getItemVNum=player.GetItemIndex
		getItemCount=player.GetItemCount
		setItemVNum=self.wndEquip.SetItemSlot
		itemCount = getItemCount(slotNumber)
		if itemCount <= 1:
			itemCount = 0
		setItemVNum(slotNumber, getItemVNum(slotNumber), itemCount)
		return

	def RefreshEquipSlotWindow(self):
		SetItemSlotVnum = self.SetItemSlotVnum
		for i in xrange(player.EQUIPMENT_PAGE_COUNT):
			SetItemSlotVnum(player.EQUIPMENT_SLOT_START + i)

		if app.ENABLE_NEW_EQUIPMENT_SYSTEM:
			SetItemSlotVnum(item.EQUIPMENT_BELT)
		if app.ENABLE_PENDANT_SYSTEM:
			SetItemSlotVnum(item.EQUIPMENT_PENDANT)
		if app.ENABLE_GLOVE_SYSTEM:
			SetItemSlotVnum(item.EQUIPMENT_GLOVE)

		self.wndEquip.RefreshSlot()

		if self.wndCostume:
			self.wndCostume.RefreshCostumeSlot()

	def RefreshItemSlot(self):
		self.RefreshBagSlotWindow()
		self.RefreshEquipSlotWindow()
		if self.wndHorseInventory:
			self.RefreshBagSlotWindow(self.wndHorseInventory.itemSlot)

	def RefreshGold(self, gold=-1):
		if gold < 0:
			gold = player.GetGold()

		self.wndMoney.SetText(localeInfo.NumberToMoneyString(gold))

		# if app.ENABLE_CHEQUE_SYSTEM:
		# 	cheque = player.GetCheque()
		# 	self.wndCheque.SetText(localeInfo.NumberToGoldNotText(cheque))

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def SellItem(self):
		if self.sellingSlotitemIndex == player.GetItemIndex(self.sellingSlotNumber):
			if self.sellingSlotitemCount == player.GetItemCount(self.sellingSlotNumber):
				net.SendShopSellPacketNew(self.sellingSlotNumber, self.questionDialog.count, player.INVENTORY)
				snd.PlaySound("sound/ui/money.wav")
		self.OnCloseQuestionDialog()

	def OnDetachMetinFromItem(self):
		if None == self.questionDialog:
			return

		#net.SendItemUseToItemPacket(self.questionDialog.sourcePos, self.questionDialog.targetPos)
		self.__SendUseItemToItemPacket(self.questionDialog.sourcePos, self.questionDialog.targetPos)
		self.OnCloseQuestionDialog()

	def OnCloseQuestionDialog(self):
		if not self.questionDialog:
			return

		self.questionDialog.Close()
		self.questionDialog = None
		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	## Slot Event
	def SelectEmptySlot(self, selectedSlotPos):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		selectedSlotPos = self.__InventoryLocalSlotPosToGlobalSlotPos(selectedSlotPos)

		if mouseModule.mouseController.isAttached():

			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			attachedItemCount = mouseModule.mouseController.GetAttachedItemCount()
			attachedItemIndex = mouseModule.mouseController.GetAttachedItemIndex()

			# The companion's item (uisidekickinventory.py): "/towarzysz eq wez".
			import uisidekickinventory
			if uisidekickinventory.DropIntoPlayerBag(attachedSlotType, attachedSlotPos, selectedSlotPos):
				mouseModule.mouseController.DeattachObject()
				return

			if player.SLOT_TYPE_INVENTORY == attachedSlotType:
				#@fixme011 BEGIN (block ds equip)
				attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)
				if player.IsDSEquipmentSlot(attachedInvenType, attachedSlotPos):
					mouseModule.mouseController.DeattachObject()
					return
				#@fixme011 END

				itemCount = player.GetItemCount(attachedSlotPos)
				attachedCount = mouseModule.mouseController.GetAttachedItemCount()
				self.__SendMoveItemPacket(attachedSlotPos, selectedSlotPos, attachedCount)

				if item.IsRefineScroll(attachedItemIndex):
					self.wndItem.SetUseMode(False)

			elif player.SLOT_TYPE_PRIVATE_SHOP == attachedSlotType:
				mouseModule.mouseController.RunCallBack("INVENTORY")

			elif player.SLOT_TYPE_SHOP == attachedSlotType:
				net.SendShopBuyPacket(attachedSlotPos)

			elif player.SLOT_TYPE_SAFEBOX == attachedSlotType:

				if player.ITEM_MONEY == attachedItemIndex:
					net.SendSafeboxWithdrawMoneyPacket(mouseModule.mouseController.GetAttachedItemCount())
					snd.PlaySound("sound/ui/money.wav")

				else:
					import safeboxtransfer
					safeboxtransfer.DropIntoBag(attachedSlotPos, selectedSlotPos, attachedItemCount, False)

			elif player.SLOT_TYPE_MALL == attachedSlotType:
				net.SendMallCheckoutPacket(attachedSlotPos, selectedSlotPos)

			mouseModule.mouseController.DeattachObject()

	def SelectItemSlot(self, itemSlotIndex):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		if itemSlotIndex >= player.INVENTORY_DEFAULT_MAX_NUM and itemSlotIndex < player.INVENTORY_MAX_NUM and not constInfo.IS_HORSE_SUMMONED:
			return

		itemSlotIndex = self.__InventoryLocalSlotPosToGlobalSlotPos(itemSlotIndex)

		if mouseModule.mouseController.isAttached():
			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			attachedItemVID = mouseModule.mouseController.GetAttachedItemIndex()

			# The companion's item (uisidekickinventory.py): "/towarzysz eq wez".
			import uisidekickinventory
			if uisidekickinventory.DropIntoPlayerBag(attachedSlotType, attachedSlotPos, itemSlotIndex):
				mouseModule.mouseController.DeattachObject()
				return

			if player.SLOT_TYPE_INVENTORY == attachedSlotType:
				#@fixme011 BEGIN (block ds equip)
				attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)
				if player.IsDSEquipmentSlot(attachedInvenType, attachedSlotPos):
					mouseModule.mouseController.DeattachObject()
					return
				#@fixme011 END
				self.__DropSrcItemToDestItemInInventory(attachedItemVID, attachedSlotPos, itemSlotIndex)

			elif player.SLOT_TYPE_SAFEBOX == attachedSlotType and player.ITEM_MONEY != attachedItemVID:
				import safeboxtransfer
				safeboxtransfer.DropIntoBag(attachedSlotPos, itemSlotIndex, mouseModule.mouseController.GetAttachedItemCount(), True)

			mouseModule.mouseController.DeattachObject()

		else:

			curCursorNum = app.GetCursor()
			if app.SELL == curCursorNum:
				self.__SellItem(itemSlotIndex)

			elif app.BUY == curCursorNum:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_BUY_INFO)

			elif app.IsPressed(app.DIK_LALT):
				link = player.GetItemLink(itemSlotIndex)
				ime.PasteString(link)

			elif app.IsPressed(app.DIK_LSHIFT):
				itemCount = player.GetItemCount(itemSlotIndex)

				if app.ENABLE_CHEQUE_SYSTEM:
					if itemCount > 1:
						self.dlgPickETC.SetTitleName(localeInfo.PICK_ITEM_TITLE)
						self.dlgPickETC.SetAcceptEvent(ui.__mem_func__(self.OnPickItem))
						self.dlgPickETC.Open(itemCount)
						self.dlgPickETC.itemGlobalSlotIndex = itemSlotIndex
				else:
					if itemCount > 1:
						self.dlgPickMoney.SetTitleName(localeInfo.PICK_ITEM_TITLE)
						self.dlgPickMoney.SetAcceptEvent(ui.__mem_func__(self.OnPickItem))
						self.dlgPickMoney.Open(itemCount)
						self.dlgPickMoney.itemGlobalSlotIndex = itemSlotIndex
				#else:
					#selectedItemVNum = player.GetItemIndex(itemSlotIndex)
					#mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_INVENTORY, itemSlotIndex, selectedItemVNum)

			elif app.IsPressed(app.DIK_LCONTROL):
				itemIndex = player.GetItemIndex(itemSlotIndex)

				if True == item.CanAddToQuickSlotItem(itemIndex):
					player.RequestAddToEmptyLocalQuickSlot(player.SLOT_TYPE_INVENTORY, itemSlotIndex)
				else:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.QUICKSLOT_REGISTER_DISABLE_ITEM)

			else:
				selectedItemVNum = player.GetItemIndex(itemSlotIndex)
				itemCount = player.GetItemCount(itemSlotIndex)

				mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_INVENTORY, itemSlotIndex, selectedItemVNum, itemCount)

				if self.__IsUsableItemToItem(selectedItemVNum, itemSlotIndex):
					self.wndItem.SetUseMode(True)
				else:
					self.wndItem.SetUseMode(False)

				snd.PlaySound("sound/ui/pick.wav")

	if app.ENABLE_CHEQUE_SYSTEM:
		def OverInToolTip(self, arg):
			arglen = len(str(arg))
			pos_x, pos_y = wndMgr.GetMousePosition()

			self.toolTip.ClearToolTip()
			self.toolTip.SetThinBoardSize(11 * arglen)
			self.toolTip.SetToolTipPosition(pos_x + 5, pos_y - 5)
			self.toolTip.AppendTextLine(arg, 0xffffff00)
			self.toolTip.Show()

		def OverOutToolTip(self):
			self.toolTip.Hide()

		def EventProgress(self, event_type, idx):
			if "mouse_over_in" == str(event_type):
				if idx == 0 :
					self.OverInToolTip(localeInfo.CHEQUE_SYSTEM_UNIT_YANG)
				elif idx == 1 :
					self.OverInToolTip(localeInfo.CHEQUE_SYSTEM_UNIT_WON)
				else:
					return
			elif "mouse_over_out" == str(event_type) :
				self.OverOutToolTip()
			else:
				return

	def __DropSrcItemToDestItemInInventory(self, srcItemVID, srcItemSlotPos, dstItemSlotPos):
		if srcItemSlotPos == dstItemSlotPos:
			return

		# cyh itemseal 2013 11 08
		if app.ENABLE_SOULBIND_SYSTEM and item.IsSealScroll(srcItemVID):
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		elif item.IsRefineScroll(srcItemVID):
			if constInfo.ENABLE_SELF_STACK_SCROLLS and player.GetItemIndex(srcItemSlotPos) == player.GetItemIndex(dstItemSlotPos):
				self.__SendMoveItemPacket(srcItemSlotPos, dstItemSlotPos,0)
			else:
				self.RefineItem(srcItemSlotPos, dstItemSlotPos)
				self.wndItem.SetUseMode(False)

		elif item.IsMetin(srcItemVID):
			self.AttachMetinToItem(srcItemSlotPos, dstItemSlotPos)

		elif item.IsDetachScroll(srcItemVID):
			self.DetachMetinFromItem(srcItemSlotPos, dstItemSlotPos)

		elif item.IsKey(srcItemVID):
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		elif (player.GetItemFlags(srcItemSlotPos) & ITEM_FLAG_APPLICABLE) == ITEM_FLAG_APPLICABLE:
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		elif item.GetUseType(srcItemVID) in self.USE_TYPE_TUPLE:
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		elif constInfo.ENABLE_SELF_STACK_SCROLLS and srcItemVID in (71052,71051,71084,71085):
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		else:
			#snd.PlaySound("sound/ui/drop.wav")

			if player.IsEquipmentSlot(dstItemSlotPos):

				if item.IsEquipmentVID(srcItemVID):
					self.__UseItem(srcItemSlotPos)

			else:
				self.__SendMoveItemPacket(srcItemSlotPos, dstItemSlotPos, 0)
				#net.SendItemMovePacket(srcItemSlotPos, dstItemSlotPos, 0)

	def __SellItem(self, itemSlotPos):
		if not player.IsEquipmentSlot(itemSlotPos):
			if constInfo.SHOP_MASS_SELL:
				eventManager.EventManager().send_event(uiShop.EVENT_ADD_MASS_SELL, itemSlotPos, player.INVENTORY)
				return

			self.sellingSlotNumber = itemSlotPos
			itemIndex = player.GetItemIndex(itemSlotPos)
			itemCount = player.GetItemCount(itemSlotPos)


			self.sellingSlotitemIndex = itemIndex
			self.sellingSlotitemCount = itemCount

			item.SelectItem(itemIndex)
			## 20140220
			if item.IsAntiFlag(item.ANTIFLAG_SELL):
				popup = uiCommon.PopupDialog()
				popup.SetText(localeInfo.SHOP_CANNOT_SELL_ITEM)
				popup.SetAcceptEvent(self.__OnClosePopupDialog)
				popup.Open()
				self.popup = popup
				return

			itemPrice = item.GetISellItemPrice()

			if item.Is1GoldItem():
				itemPrice = itemCount / itemPrice
			else:
				itemPrice = itemPrice * itemCount

			if not app.ENABLE_NO_SELL_PRICE_DIVIDED_BY_5:
				isGoldBar = itemIndex >= 80003 and itemIndex <= 80008

				if not isGoldBar:
					itemPrice /= 5

			item.GetItemName(itemIndex)
			itemName = item.GetItemName()

			self.questionDialog = uiCommon.QuestionDialog()
			self.questionDialog.SetText(localeInfo.DO_YOU_SELL_ITEM(itemName, itemCount, itemPrice))
			self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.SellItem))
			self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
			self.questionDialog.Open()
			self.questionDialog.count = itemCount

			constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

	def __OnClosePopupDialog(self):
		self.pop = None

	def RefineItem(self, scrollSlotPos, targetSlotPos):

		scrollIndex = player.GetItemIndex(scrollSlotPos)
		targetIndex = player.GetItemIndex(targetSlotPos)

		if player.REFINE_OK != player.CanRefine(scrollIndex, targetSlotPos):
			return

		###########################################################
		self.__SendUseItemToItemPacket(scrollSlotPos, targetSlotPos)
		#net.SendItemUseToItemPacket(scrollSlotPos, targetSlotPos)
		return

	def DetachMetinFromItem(self, scrollSlotPos, targetSlotPos):
		scrollIndex = player.GetItemIndex(scrollSlotPos)
		targetIndex = player.GetItemIndex(targetSlotPos)

		if not player.CanDetach(scrollIndex, targetSlotPos):
			if app.ENABLE_ACCE_COSTUME_SYSTEM:
				item.SelectItem(scrollIndex)
				if item.GetValue(0) == acce.CLEAN_ATTR_VALUE0:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.ACCE_FAILURE_CLEAN)
				else:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_METIN_INSEPARABLE_ITEM)
			else:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_METIN_INSEPARABLE_ITEM)
			return

		self.questionDialog = uiCommon.QuestionDialog()
		self.questionDialog.SetText(localeInfo.REFINE_DO_YOU_SEPARATE_METIN)
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			item.SelectItem(targetIndex)
			if item.GetItemType() == item.ITEM_TYPE_COSTUME and item.GetItemSubType() == item.COSTUME_TYPE_ACCE:
				item.SelectItem(scrollIndex)
				if item.GetValue(0) == acce.CLEAN_ATTR_VALUE0:
					self.questionDialog.SetText(localeInfo.ACCE_DO_YOU_CLEAN)

		self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.OnDetachMetinFromItem))
		self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
		self.questionDialog.Open()
		self.questionDialog.sourcePos = scrollSlotPos
		self.questionDialog.targetPos = targetSlotPos

	def AttachMetinToItem(self, metinSlotPos, targetSlotPos):
		metinIndex = player.GetItemIndex(metinSlotPos)
		targetIndex = player.GetItemIndex(targetSlotPos)

		item.SelectItem(metinIndex)
		itemName = item.GetItemName()

		result = player.CanAttachMetin(metinIndex, targetSlotPos)

		if player.ATTACH_METIN_NOT_MATCHABLE_ITEM == result:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_CAN_NOT_ATTACH(itemName))

		if player.ATTACH_METIN_NO_MATCHABLE_SOCKET == result:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_NO_SOCKET(itemName))

		elif player.ATTACH_METIN_NOT_EXIST_GOLD_SOCKET == result:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_NO_GOLD_SOCKET(itemName))

		elif player.ATTACH_METIN_CANT_ATTACH_TO_EQUIPMENT == result:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_EQUIP_ITEM)

		if player.ATTACH_METIN_OK != result:
			return

		self.attachMetinDialog.Open(metinSlotPos, targetSlotPos)



	def OverOutItem(self):
		self.wndItem.SetUsableItem(False)
		if None != self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OverInItem(self, overSlotPos):
		self.inventorySlotStateMgr.OnSlotMouseOverIn(overSlotPos)
		overSlotPosGlobal = self.__InventoryLocalSlotPosToGlobalSlotPos(overSlotPos)
		self.wndItem.SetUsableItem(False)

		if mouseModule.mouseController.isAttached():
			attachedItemType = mouseModule.mouseController.GetAttachedType()
			if player.SLOT_TYPE_INVENTORY == attachedItemType:

				attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
				attachedItemVNum = mouseModule.mouseController.GetAttachedItemIndex()

				if attachedItemVNum==player.ITEM_MONEY: # @fixme005
					pass
				elif self.__CanUseSrcItemToDstItem(attachedItemVNum, attachedSlotPos, overSlotPosGlobal):
					self.wndItem.SetUsableItem(True)
					self.ShowToolTip(overSlotPosGlobal)
					return

		self.ShowToolTip(overSlotPosGlobal)


	def __IsUsableItemToItem(self, srcItemVNum, srcSlotPos):
		if item.IsRefineScroll(srcItemVNum):
			return True
		elif item.IsMetin(srcItemVNum):
			return True
		elif item.IsDetachScroll(srcItemVNum):
			return True
		elif item.IsKey(srcItemVNum):
			return True
		elif (player.GetItemFlags(srcSlotPos) & ITEM_FLAG_APPLICABLE) == ITEM_FLAG_APPLICABLE:
			return True
		elif constInfo.ENABLE_SELF_STACK_SCROLLS and srcItemVNum in (71052,71051,71084,71085):
			return True
		else:
			if item.GetUseType(srcItemVNum) in self.USE_TYPE_TUPLE:
				return True

		return False

	def __CanUseSrcItemToDstItem(self, srcItemVNum, srcSlotPos, dstSlotPos):
		if srcSlotPos == dstSlotPos:
			return False

		if item.IsRefineScroll(srcItemVNum):
			if player.REFINE_OK == player.CanRefine(srcItemVNum, dstSlotPos):
				return True
			elif constInfo.ENABLE_SELF_STACK_SCROLLS and player.GetItemIndex(dstSlotPos) == srcItemVNum:
				return True
		elif item.IsMetin(srcItemVNum):
			if player.ATTACH_METIN_OK == player.CanAttachMetin(srcItemVNum, dstSlotPos):
				return True
		elif item.IsDetachScroll(srcItemVNum):
			if player.DETACH_METIN_OK == player.CanDetach(srcItemVNum, dstSlotPos):
				return True
		elif item.IsKey(srcItemVNum):
			if player.CanUnlock(srcItemVNum, dstSlotPos):
				return True

		elif (player.GetItemFlags(srcSlotPos) & ITEM_FLAG_APPLICABLE) == ITEM_FLAG_APPLICABLE:
			return True

		elif constInfo.ENABLE_SELF_STACK_SCROLLS and srcItemVNum in (71052,71051,71084,71085):
			return True

		else:
			useType=item.GetUseType(srcItemVNum)

			if "USE_CLEAN_SOCKET" == useType:
				if self.__CanCleanBrokenMetinStone(dstSlotPos):
					return True
			elif "USE_CHANGE_ATTRIBUTE" == useType:
				if self.__CanChangeItemAttrList(dstSlotPos):
					return True
			elif "USE_ADD_ATTRIBUTE" == useType:
				if self.__CanAddItemAttr(dstSlotPos):
					return True
			elif "USE_ADD_ATTRIBUTE2" == useType:
				if self.__CanAddItemAttr(dstSlotPos):
					return True
			elif "USE_ADD_ACCESSORY_SOCKET" == useType:
				if self.__CanAddAccessorySocket(dstSlotPos):
					return True
			elif "USE_PUT_INTO_ACCESSORY_SOCKET" == useType:
				if self.__CanPutAccessorySocket(dstSlotPos, srcItemVNum):
					return True;
			elif "USE_PUT_INTO_BELT_SOCKET" == useType:
				dstItemVNum = player.GetItemIndex(dstSlotPos)
				print "USE_PUT_INTO_BELT_SOCKET", srcItemVNum, dstItemVNum

				item.SelectItem(dstItemVNum)

				if item.ITEM_TYPE_BELT == item.GetItemType():
					return True
			elif app.ENABLE_USE_COSTUME_ATTR and "USE_CHANGE_COSTUME_ATTR" == useType:
				if self.__CanChangeCostumeAttrList(dstSlotPos):
					return True
			elif app.ENABLE_USE_COSTUME_ATTR and "USE_RESET_COSTUME_ATTR" == useType:
				if self.__CanResetCostumeAttr(dstSlotPos):
					return True

		return False

	def __CanCleanBrokenMetinStone(self, dstSlotPos):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if item.ITEM_TYPE_WEAPON != item.GetItemType():
			return False

		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			if player.GetItemMetinSocket(dstSlotPos, i) == constInfo.ERROR_METIN_STONE:
				return True

		return False

	def __CanChangeItemAttrList(self, dstSlotPos):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if not item.GetItemType() in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
			return False

		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			if player.GetItemAttribute(dstSlotPos, i)[0] != 0:
				return True

		return False

	if app.ENABLE_USE_COSTUME_ATTR:
		def __CanChangeCostumeAttrList(self, dstSlotPos):
			dstItemVNum = player.GetItemIndex(dstSlotPos)
			if dstItemVNum == 0:
				return False

			item.SelectItem(dstItemVNum)

			if item.GetItemType() != item.ITEM_TYPE_COSTUME:
				return False

			for i in xrange(player.METIN_SOCKET_MAX_NUM):
				if player.GetItemAttribute(dstSlotPos, i)[0] != 0:
					return True

			return False

		def __CanResetCostumeAttr(self, dstSlotPos):
			dstItemVNum = player.GetItemIndex(dstSlotPos)
			if dstItemVNum == 0:
				return False

			item.SelectItem(dstItemVNum)

			if item.GetItemType() != item.ITEM_TYPE_COSTUME:
				return False

			for i in xrange(player.METIN_SOCKET_MAX_NUM):
				if player.GetItemAttribute(dstSlotPos, i)[0] != 0:
					return True

			return False

	def __CanPutAccessorySocket(self, dstSlotPos, mtrlVnum):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if item.GetItemType() != item.ITEM_TYPE_ARMOR:
			return False

		if not item.GetItemSubType() in (item.ARMOR_WRIST, item.ARMOR_NECK, item.ARMOR_EAR):
			return False

		curCount = player.GetItemMetinSocket(dstSlotPos, 0)
		maxCount = player.GetItemMetinSocket(dstSlotPos, 1)

		if mtrlVnum != constInfo.GET_ACCESSORY_MATERIAL_VNUM(dstItemVNum, item.GetItemSubType()):
			return False

		if curCount>=maxCount:
			return False

		return True

	def __CanAddAccessorySocket(self, dstSlotPos):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if item.GetItemType() != item.ITEM_TYPE_ARMOR:
			return False

		if not item.GetItemSubType() in (item.ARMOR_WRIST, item.ARMOR_NECK, item.ARMOR_EAR):
			return False

		curCount = player.GetItemMetinSocket(dstSlotPos, 0)
		maxCount = player.GetItemMetinSocket(dstSlotPos, 1)

		ACCESSORY_SOCKET_MAX_SIZE = 3
		if maxCount >= ACCESSORY_SOCKET_MAX_SIZE:
			return False

		return True

	def __CanAddItemAttr(self, dstSlotPos):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if not item.GetItemType() in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
			return False

		attrCount = 0
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			if player.GetItemAttribute(dstSlotPos, i)[0] != 0:
				attrCount += 1

		if attrCount<4:
			return True

		return False

	def ShowToolTip(self, slotIndex):
		if None != self.tooltipItem:
			self.tooltipItem.SetInventoryItem(slotIndex)

			if self.isOfflineShopManageOpen or self.isOfflineShopBuilderOpen:
				self.tooltipItem.AppendSpace(5)
				self.tooltipItem.AppendTextLine(localeInfo.QUICK_ADD_TO_MYSHOP)

	def OnTop(self):
		if None != self.tooltipItem:
			self.tooltipItem.SetTop()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def UseItemSlot(self, slotIndex):
		curCursorNum = app.GetCursor()
		if app.SELL == curCursorNum:
			return

		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS():
			return

		slotIndex = self.__InventoryLocalSlotPosToGlobalSlotPos(slotIndex)

		if self.isExchangeDialogOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "exchange", slotIndex)
			return
		elif self.isOfflineShopBuilderOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "myshop", slotIndex)
			return
		elif self.isOfflineShopManageOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "myshop_manage", slotIndex)
			return
		elif self.isSafeboxOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "safebox", slotIndex)
			return
		elif self.isExchangeItemOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "item_exchange", slotIndex)
			return
		elif self.isRechargePotion:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "recharge_potion", slotIndex)
			return

		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			if self.wndDragonSoulRefine.IsShow():
				self.wndDragonSoulRefine.AutoSetItem((player.INVENTORY, slotIndex), 1)
				return
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			if self.isShowAcceWindow():
				acce.Add(player.INVENTORY, slotIndex, 255)
				return

		self.__UseItem(slotIndex)
		mouseModule.mouseController.DeattachObject()
		self.OverOutItem()

	def __UseItem(self, slotIndex):
		ItemVNum = player.GetItemIndex(slotIndex)
		item.SelectItem(ItemVNum)
		if item.IsFlag(item.ITEM_FLAG_CONFIRM_WHEN_USE):
			self.questionDialog = uiCommon.QuestionDialog()
			self.questionDialog.SetText(localeInfo.INVENTORY_REALLY_USE_ITEM)
			self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.__UseItemQuestionDialog_OnAccept))
			self.questionDialog.SetCancelEvent(ui.__mem_func__(self.__UseItemQuestionDialog_OnCancel))
			self.questionDialog.Open()
			self.questionDialog.slotIndex = slotIndex

			constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

		else:
			self.__SendUseItemPacket(slotIndex)
			#net.SendItemUsePacket(slotIndex)

	def __UseItemQuestionDialog_OnCancel(self):
		self.OnCloseQuestionDialog()

	def __UseItemQuestionDialog_OnAccept(self):
		self.__SendUseItemPacket(self.questionDialog.slotIndex)
		self.OnCloseQuestionDialog()

	def __SendUseItemToItemPacket(self, srcSlotPos, dstSlotPos):
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.USE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		dstItemVNum = player.GetItemIndex(dstSlotPos)
		srcItemVNum = player.GetItemIndex(srcSlotPos)
		if constInfo.ENABLE_SELF_STACK_SCROLLS and dstItemVNum == srcItemVNum:
			self.__SendMoveItemPacket(srcSlotPos, dstSlotPos, 0)
		else:
			net.SendItemUseToItemPacket(srcSlotPos, dstSlotPos)

	def __SendUseItemPacket(self, slotPos):
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.USE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		net.SendItemUsePacket(slotPos)

	def __SendMoveItemPacket(self, srcSlotPos, dstSlotPos, srcItemCount):
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.MOVE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		net.SendItemMovePacket(srcSlotPos, dstSlotPos, srcItemCount)

	def SetDragonSoulRefineWindow(self, wndDragonSoulRefine):
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoulRefine = wndDragonSoulRefine

	def OnMoveWindow(self, x, y):
		# print "Inventory Global Pos : ", self.GetGlobalPosition()
		if self.wndBelt:
			# print "Belt Global Pos : ", self.wndBelt.GetGlobalPosition()
			self.wndBelt.AdjustPositionAndSize()

	if app.ENABLE_ACCE_COSTUME_SYSTEM:
		def SetAcceWindow(self, wndAcceCombine, wndAcceAbsorption):
			self.wndAcceCombine = wndAcceCombine
			self.wndAcceAbsorption = wndAcceAbsorption

		def isShowAcceWindow(self):
			if self.wndAcceCombine:
				if self.wndAcceCombine.IsShow():
					return 1
			if self.wndAcceAbsorption:
				if self.wndAcceAbsorption.IsShow():
					return 1
			return 0
