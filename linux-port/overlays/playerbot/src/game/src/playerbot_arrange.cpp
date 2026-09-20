// "Scal i uporzadkuj" in the engine (playerbot_arrange.h): the bag read into
// the planner's terms (playerbot_arrange_rules.h), and a complete plan applied
// the way MoveItem moves one item - RemoveFromCharacter, then SetItem at the
// new cell - so the grid, the packets to the client and the delayed save are
// the engine's own. Nothing is created from a vnum and nothing is thrown away
// but a stack that another has taken all of, which is what MoveItem's own
// stacking does.
//
// The order of work is the one Codex's audit of 18 September asked for:
// every refusal is made before the first item moves, the plan is complete and
// checked before it is used, and the quickslots are rewritten from what they
// said before, not from whatever the stack pouring left them saying.
//
// Every row the operation changed is written in the same second (FlushRow):
// the delayed save would get there too, but the db core's cache writes each
// item on its own five-minute clock, and in between the database held a bag
// half in its old cells and half in its new, and a stack poured into beside
// the deleted row of the stack it emptied. What is still not promised is a
// db core that dies while those rows are being written: CInputDB::ItemLoad
// then puts an item whose cell is taken into the first free one
// (ITEM_RESTORE), as it does after any interrupted move.
#include "stdafx.h"
#include "playerbot_arrange.h"

#if defined(PLAYERBOT_ENGINE_MT2009)

#include "utils.h"
#include "config.h"
#include "char.h"
#include "item.h"
#include "item_manager.h"
#include "questmanager.h"
#include "questpc.h"
#include "desc_client.h"
#include "safebox.h"
#include "log.h"
#include "unique_item.h"
#include "belt_inventory_helper.h"
#include "../../common/CommonDefines.h"
#include "../../common/PulseManager.h"
#include "playerbot_arrange_rules.h"

#include <chrono>
#include <cstring>
#include <iterator>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

namespace playerbot_arrange {
namespace {

namespace rules = playerbot_arrange_rules;

static_assert(INVENTORY_PAGE_COLUMN == rules::PAGE_COLUMNS, "the planner's page is five columns wide");
static_assert(INVENTORY_PAGE_ROW == rules::PAGE_ROWS, "the planner's page is nine rows tall");
static_assert(INVENTORY_DEFAULT_MAX_NUM == INVENTORY_DEFAULT_PAGE_COUNT * INVENTORY_PAGE_SIZE,
		"the bag is whole pages");

// A second click inside this is answered, not acted on: the server's own
// limit, whatever the client's button does.
const DWORD PLAYER_COOLDOWN_MS = 2000;

std::unordered_map<DWORD, DWORD> s_mapLastPlayerArrange;

// The order of the bag, category first. Potions lead - "wszelakie potki
// pierwsze a potem reszte" was the operator's rule for the bots' bags (Tieru,
// 15 September), and a player reaches for them most - then what is worn, the
// books, what improves gear, chests and keys, the other usable things, what
// fishing and gathering need, quest items, and the rest.
enum ECategory {
	CAT_POTION = 0,
	CAT_WEAPON,
	CAT_ARMOR,
	CAT_BOOK,
	CAT_UPGRADE,
	CAT_CHEST,
	CAT_USE,
	CAT_GATHER,
	CAT_QUEST,
	CAT_OTHER,
};

// Eliksir Slonca and Eliksir Ksiezyca: ITEM_USE, USE_SPECIAL, switched on and
// off by a use (char_item.cpp). Potions for the purpose of the order.
bool IsAutoPotionVnum(DWORD vnum)
{
	return (vnum >= 72723 && vnum <= 72730) || vnum == 76004 || vnum == 76005 ||
			vnum == 76021 || vnum == 76022 || vnum == 79012 || vnum == 79013;
}

// Sztuka Combo and the Leadership books: ITEM_USE, USE_SPECIAL, read like a
// book.
bool IsGeneralSkillBookVnum(DWORD vnum)
{
	return vnum >= 50301 && vnum <= 50306;
}

void SortKeyOf(LPITEM item, int64_t* key)
{
	const BYTE type = item->GetType();
	const BYTE sub = item->GetSubType();
	const DWORD vnum = item->GetVnum();
	int category = CAT_OTHER;
	int rank = type;
	switch (type) {
	case ITEM_USE:
		rank = 0;
		switch (sub) {
		case USE_POTION:
		case USE_POTION_NODELAY:
			category = CAT_POTION;
			rank = 0;
			break;
		case USE_POTION_CONTINUE:
		case USE_ABILITY_UP:
			category = CAT_POTION;
			rank = 1;
			break;
		case USE_AFFECT:
			// value0 510 is the engine's timed stat buff; 512 and 513 are no
			// potions at all (an exorcism scroll among them).
			category = item->GetValue(0) == 510 ? CAT_POTION : CAT_USE;
			rank = 2;
			break;
		case USE_SPECIAL:
			if (IsAutoPotionVnum(vnum)) {
				category = CAT_POTION;
				rank = 3;
			} else if (IsGeneralSkillBookVnum(vnum)) {
				category = CAT_BOOK;
				rank = 2;
			} else {
				category = CAT_USE;
			}
			break;
		case USE_TUNING:
		case USE_CHANGE_ATTRIBUTE:
		case USE_ADD_ATTRIBUTE:
		case USE_ADD_ATTRIBUTE2:
		case USE_CHANGE_ATTRIBUTE2:
		case USE_ADD_ACCESSORY_SOCKET:
		case USE_PUT_INTO_ACCESSORY_SOCKET:
		case USE_PUT_INTO_BELT_SOCKET:
		case USE_PUT_INTO_RING_SOCKET:
		case USE_CLEAN_SOCKET:
		case USE_CHANGE_COSTUME_ATTR:
		case USE_RESET_COSTUME_ATTR:
			category = CAT_UPGRADE;
			rank = 1;
			break;
		case USE_TREASURE_BOX:
			category = CAT_CHEST;
			break;
		case USE_BAIT:
			category = CAT_GATHER;
			break;
		default:
			category = CAT_USE;
			break;
		}
		break;
	case ITEM_POTION:
	case ITEM_BLEND:
		category = CAT_POTION;
		rank = 2;
		break;
	case ITEM_WEAPON:
		category = CAT_WEAPON;
		rank = sub == WEAPON_ARROW ? 1 : 0;
#ifdef ENABLE_QUIVER_SYSTEM
		if (sub == WEAPON_QUIVER)
			rank = 1;
#endif
		break;
	case ITEM_ARMOR:
		category = CAT_ARMOR;
		rank = sub;
		break;
	case ITEM_BELT:
	case ITEM_RING:
	case ITEM_UNIQUE:
	case ITEM_COSTUME:
		category = CAT_ARMOR;
		rank = 20 + type;
		break;
	case ITEM_SKILLBOOK:
		category = CAT_BOOK;
		rank = 0;
		break;
	case ITEM_SKILLFORGET:
		category = CAT_BOOK;
		rank = 1;
		break;
	case ITEM_METIN:
		category = CAT_UPGRADE;
		rank = 0;
		break;
	case ITEM_MATERIAL:
	case ITEM_RESOURCE:
		category = CAT_UPGRADE;
		rank = 2;
		break;
	case ITEM_TREASURE_BOX:
	case ITEM_TREASURE_KEY:
	case ITEM_GIFTBOX:
		category = CAT_CHEST;
		break;
	case ITEM_FISH:
	case ITEM_ROD:
	case ITEM_PICK:
	case ITEM_HERB_KNIFE:
	case ITEM_CAMPFIRE:
		category = CAT_GATHER;
		break;
	case ITEM_QUEST:
		category = CAT_QUEST;
		break;
	default:
		break;
	}
	key[0] = category;
	key[1] = rank;
	key[2] = type;
	key[3] = sub;
	key[4] = vnum;
	// A book by its skill (vnum 50300 carries the skill in socket 0); every
	// other kind with the full stacks before the partial one.
	key[5] = (type == ITEM_SKILLBOOK || type == ITEM_SKILLFORGET) ? item->GetSocket(0)
			: -(int64_t)item->GetCount();
}

// Whether two bag stacks may pour into each other. The engine's rule
// (MoveItem, AutoStackItem) is the vnum, the stack flag and every socket; this
// asks for the attributes, the flag word and the look as well, so it never
// pours what the engine would not, and never what a player could tell apart.
bool SameStack(LPITEM a, LPITEM b)
{
	if (a->GetVnum() != b->GetVnum() || a->GetOriginalVnum() != b->GetOriginalVnum())
		return false;
	if (!a->IsStackable() || IS_SET(a->GetAntiFlag(), ITEM_ANTIFLAG_STACK) ||
			!b->IsStackable() || IS_SET(b->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
		return false;
	if (a->GetFlag() != b->GetFlag())
		return false;
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		if (a->GetSocket(i) != b->GetSocket(i))
			return false;
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		if (a->GetAttributeType(i) != b->GetAttributeType(i) || a->GetAttributeValue(i) != b->GetAttributeValue(i))
			return false;
	if (a->GetMaskVnum() != b->GetMaskVnum() || a->GetSIGVnum() != b->GetSIGVnum())
		return false;
	return true;
}

// An item's row written now, not when the db core's cache gets round to it
// (ITEM_CACHE_FLUSH_SECONDS, five minutes, counted per item): the delayed save
// sends the item, then HEADER_GD_ITEM_FLUSH has the db core write the row -
// FlushPlayerBotItemRow's shape. Measured on the test world before it: an
// arranged bag stood in the database half in its old cells and half in its
// new ones for up to five minutes (81 bags with two items on one cell a
// minute after 400 arranges), and a stack another was poured into kept its
// old count there while the emptied one's row was already deleted
// (DestroyItem writes at once) - a crash inside that window lost the
// units poured. The rows would be written within five minutes anyway; this
// writes them in the same second as the arrange.
void FlushRow(LPITEM item)
{
	if (!item || item->GetID() == 0 || !db_clientdesc)
		return;
	ITEM_MANAGER::instance().FlushDelayedSave(item);
	const DWORD id = item->GetID();
	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_FLUSH, 0, sizeof(DWORD));
	db_clientdesc->Packet(&id, sizeof(DWORD));
}

// An item a plan was sure of and the engine did not put where it was told. Not
// expected - SetItem refuses only an item with an owner or a cell outside the
// window, and neither can happen here - but an ownerless item is destroyed by
// the next delayed save, so it goes back into the bag, or to its owner's feet.
void Rescue(LPCHARACTER ch, LPITEM item)
{
	if (item->GetOwner())
		return;
	const int cell = ch->GetEmptyInventory(item->GetSize());
	if (cell >= 0) {
		item->AddToCharacter(ch, TItemPos(INVENTORY, (WORD)cell));
		sys_err("INVENTORY_ARRANGE: pid=%u name=%s item %u (%s) put back at cell %d",
				ch->GetPlayerID(), ch->GetName(), item->GetID(), item->GetName(), cell);
		return;
	}
	PIXEL_POSITION pos;
	pos.x = ch->GetX();
	pos.y = ch->GetY();
	item->AddToGround(ch->GetMapIndex(), pos);
	item->SetOwnership(ch, 300);
	item->StartDestroyEvent();
	sys_err("INVENTORY_ARRANGE: pid=%u name=%s item %u (%s) had no cell and was put at the owner's feet",
			ch->GetPlayerID(), ch->GetName(), item->GetID(), item->GetName());
}

// One cell of the bag, said again to the client exactly as CHARACTER::SetItem
// says it: ITEM_SET for what stands there, ITEM_DEL for a cell that is empty.
// Only ever a copy of what the server holds, so it cannot introduce a state of
// its own; the whole point is that the client ends the operation agreeing.
void RestateCell(LPCHARACTER ch, WORD cell)
{
	LPDESC desc = ch->GetDesc();
	if (!desc)
		return;
	LPITEM item = ch->GetInventoryItem(cell);
	if (item) {
		TPacketGCItemSet pack;
		memset(&pack, 0, sizeof(pack));
		pack.header = HEADER_GC_ITEM_SET;
		pack.Cell = TItemPos(INVENTORY, cell);
		pack.count = item->GetCount();
		pack.vnum = item->GetVnum();
		pack.flags = item->GetFlag();
		pack.anti_flags = item->GetAntiFlag();
#ifdef ENABLE_HIGHLIGHT_NEW_ITEM
		pack.highlight = false;
#endif
		thecore_memcpy(pack.alSockets, item->GetSockets(), sizeof(pack.alSockets));
		thecore_memcpy(pack.aAttr, item->GetAttributes(), sizeof(pack.aAttr));
		desc->Packet(&pack, sizeof(pack));
		return;
	}
	TPacketGCItemDelDeprecated pack;
	memset(&pack, 0, sizeof(pack));
	pack.header = HEADER_GC_ITEM_DEL;
	pack.Cell = TItemPos(INVENTORY, cell);
	desc->Packet(&pack, sizeof(pack));
}

// An empty cell the grid still calls taken, with nothing above it to explain
// the mark. SetItem writes the item's pointer into its top cell alone while
// bItemGrid is marked for every cell the piece covers, so "no pointer here,
// and the grid says taken" is the ordinary state of the lower half of every
// sword and every breastplate. Counting those called 2188 healthy bags broken
// on the test world - and not one of them differed before and after, which is
// what said the measurement was wrong rather than the bags. The footprint is
// taken first and only what it cannot account for is a hole.
int CountPlayerBotGridHoles(LPCHARACTER ch, WORD cells)
{
	std::vector<uint8_t> covered(cells, 0);
	for (WORD cell = 0; cell < cells; ++cell) {
		LPITEM item = ch->GetInventoryItem(cell);
		if (!item)
			continue;
		const TItemTable* proto = item->GetProto();
		const int height = (proto && proto->bSize > 0) ? proto->bSize : 1;
		for (int k = 0; k < height && rules::RowOf(cell) + k < rules::PAGE_ROWS; ++k) {
			const int at = (int)cell + k * rules::PAGE_COLUMNS;
			if (at >= 0 && at < (int)cells)
				covered[at] = 1;
		}
	}
	int holes = 0;
	for (WORD cell = 0; cell < cells; ++cell)
		if (!covered[cell] && !ch->IsEmptyItemGrid(TItemPos(INVENTORY, cell), 1))
			++holes;
	return holes;
}

}  // namespace

TResult ArrangeInventory(LPCHARACTER ch, bool fromPlayer)
{
	TResult result;
	if (!ch || !ch->IsPC() || !ch->IsItemLoaded()) {
		result.code = RESULT_BUSY;
		return result;
	}
	if (ch->IsDead()) {
		result.code = RESULT_DEAD;
		return result;
	}
	// Everything MoveItem asks of the character, and every busy state but the
	// safebox: an exchange, a shop and its management, the item shop, the
	// cube, crafting, a refine, the dragon soul and acce windows, a warp. The
	// safebox is let through as MoveItem lets it through (apply_safebox_hands):
	// the bag is sorted beside it and nothing here touches the box. A quest
	// running holds items it has been handed, by pointer or by cell.
	if (!ch->CanHandleItem(false, false, BUSY_SAFEBOX) ||
			quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID())->IsRunning()) {
		result.code = RESULT_BUSY;
		return result;
	}
	const DWORD now = get_dword_time();
	if (fromPlayer) {
		std::unordered_map<DWORD, DWORD>::iterator last = s_mapLastPlayerArrange.find(ch->GetPlayerID());
		if (last != s_mapLastPlayerArrange.end() && now - last->second < PLAYER_COOLDOWN_MS) {
			result.code = RESULT_COOLDOWN;
			return result;
		}
		if (s_mapLastPlayerArrange.size() > 4096)
			for (std::unordered_map<DWORD, DWORD>::iterator it = s_mapLastPlayerArrange.begin(); it != s_mapLastPlayerArrange.end();)
				it = now - it->second > PLAYER_COOLDOWN_MS ? s_mapLastPlayerArrange.erase(it) : std::next(it);
		s_mapLastPlayerArrange[ch->GetPlayerID()] = now;
	}
	const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();

	// The four pages as the engine holds them. The horse's page, the worn
	// slots, the belt and the dragon soul window are other cells and are not
	// read at all.
	std::vector<rules::Item> items;
	std::vector<LPITEM> handles;
	std::map<uint32_t, LPITEM> handleOf;
	std::map<int, uint32_t> idAtCell;
	for (WORD cell = 0; cell < INVENTORY_DEFAULT_MAX_NUM; ++cell) {
		LPITEM item = ch->GetInventoryItem(cell);
		if (!item)
			continue;
		if (item->GetCell() != cell || item->GetOwner() != ch || item->GetWindow() != INVENTORY ||
				item->GetID() == 0 || handleOf.count(item->GetID())) {
			sys_err("INVENTORY_ARRANGE: pid=%u name=%s refused: item %u (%s) at cell %u says cell %u window %u",
					ch->GetPlayerID(), ch->GetName(), item->GetID(), item->GetName(), cell,
					item->GetCell(), item->GetWindow());
			result.code = RESULT_INCONSISTENT;
			return result;
		}
		rules::Item planned;
		planned.id = item->GetID();
		planned.cell = cell;
		// Six protos on this line say size 0 (none in anybody's bag on the test
		// world). SetItem marks no grid cell for one, so it is no layout this
		// planner could make or trust: it stays where it is, as one cell,
		// and the rest of the bag is arranged round it. Nothing is taller
		// than three here; a proto that says so is a bag left alone.
		const int size = item->GetSize();
		if (size > rules::MAX_HEIGHT) {
			sys_err("INVENTORY_ARRANGE: pid=%u name=%s refused: item %u (%s) is %d cells tall",
					ch->GetPlayerID(), ch->GetName(), item->GetID(), item->GetName(), size);
			result.code = RESULT_INCONSISTENT;
			return result;
		}
		planned.height = size >= 1 ? size : 1;
		planned.count = item->GetCount();
		planned.maxStack = item->GetMaxStack();
		planned.pinned = item->isLocked() || item->IsExchanging() || planned.height != size;
		SortKeyOf(item, planned.key);
		items.push_back(planned);
		handles.push_back(item);
		handleOf[planned.id] = item;
		idAtCell[cell] = planned.id;
		if (planned.pinned)
			++result.pinned;
	}
	result.items = (int)items.size();
	// Units per vnum, to be counted again at the end: pouring moves units
	// between two stacks of one kind and nothing else, so the sums cannot
	// change, and a difference is a loss somebody has to hear about.
	std::map<DWORD, uint64_t> unitsBefore;
	for (size_t i = 0; i < handles.size(); ++i)
		unitsBefore[handles[i]->GetVnum()] += handles[i]->GetCount();

	// And the grid as the bag already carried it, so a cell that was unusable
	// before this ran is not counted against it afterwards. Counted against
	// GetInventoryMaxCount, which is what IsEmptyItemGrid itself measures by:
	// a cell past the pages this character has bought answers "not empty" with
	// nothing in it, and reading it as a hole says every bot without the full
	// bag has half a page of them.
	const WORD bagCells = std::min<WORD>(ch->GetInventoryMaxCount(), INVENTORY_DEFAULT_MAX_NUM);
	const int holesBefore = CountPlayerBotGridHoles(ch, bagCells);

	// Merge groups by comparing the items themselves.
	std::vector<LPITEM> representatives;
	for (size_t i = 0; i < items.size(); ++i) {
		LPITEM item = handles[i];
		if (items[i].pinned || !item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK) ||
				item->GetMaxStack() < 2)
			continue;
		size_t group = 0;
		for (; group < representatives.size(); ++group)
			if (SameStack(representatives[group], item))
				break;
		if (group == representatives.size())
			representatives.push_back(item);
		items[i].mergeGroup = (uint32_t)group + 1;
	}

	const rules::Plan plan = rules::MakePlan(items, INVENTORY_DEFAULT_PAGE_COUNT);
	if (!plan.ok) {
		sys_err("INVENTORY_ARRANGE: pid=%u name=%s refused: the bag is not a legal layout (%d items)",
				ch->GetPlayerID(), ch->GetName(), result.items);
		result.code = RESULT_INCONSISTENT;
		return result;
	}
	result.strategy = plan.strategy;
	if (plan.transfers.empty() && plan.moved == 0) {
		result.code = RESULT_NOTHING;
		return result;
	}

	// The quickslots as they were, to be pointed at the new cells afterwards.
	TQuickslot before[QUICKSLOT_MAX_NUM];
	std::memset(before, 0, sizeof(before));
	for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i) {
		TQuickslot* slot = NULL;
		if (ch->GetQuickslot((BYTE)i, &slot) && slot)
			before[i] = *slot;
	}

	// The pours. The receiver grows first; the giver shrinks, and at nothing
	// SetCount removes and destroys it - MoveItem's stacking, step by step.
	for (const rules::Transfer& transfer : plan.transfers) {
		std::map<uint32_t, LPITEM>::iterator from = handleOf.find(transfer.from);
		std::map<uint32_t, LPITEM>::iterator to = handleOf.find(transfer.to);
		if (from == handleOf.end() || to == handleOf.end() || from->second->GetCount() < transfer.units ||
				to->second->GetCount() + transfer.units > to->second->GetMaxStack()) {
			sys_err("INVENTORY_ARRANGE: pid=%u name=%s pour %u -> %u of %u refused; the bag stays as poured so far",
					ch->GetPlayerID(), ch->GetName(), transfer.from, transfer.to, transfer.units);
			result.code = RESULT_INCONSISTENT;
			return result;
		}
		to->second->SetCount(to->second->GetCount() + transfer.units);
		const ITEM_COUNT left = from->second->GetCount() - transfer.units;
		from->second->SetCount(left);
		if (left == 0) {
			handleOf.erase(from);
			++result.merged;
		}
		result.units += transfer.units;
	}

	// The moves: every item that changes cell leaves its cell first, then each
	// is set down in its new one. Two passes, so a cycle - A into B's cell
	// while B goes into A's - needs no free cell in between.
	std::vector<std::pair<LPITEM, WORD> > movers;
	for (const rules::Placement& placement : plan.placements) {
		std::map<uint32_t, LPITEM>::iterator it = handleOf.find(placement.id);
		if (it == handleOf.end())
			continue;
		if (it->second->GetCell() != (WORD)placement.cell)
			movers.push_back(std::make_pair(it->second, (WORD)placement.cell));
	}
	for (size_t i = 0; i < movers.size(); ++i)
		movers[i].first->RemoveFromCharacter();
	for (size_t i = 0; i < movers.size(); ++i) {
#ifdef ENABLE_HIGHLIGHT_NEW_ITEM
		ch->SetItem(TItemPos(INVENTORY, movers[i].second), movers[i].first, true);
#else
		ch->SetItem(TItemPos(INVENTORY, movers[i].second), movers[i].first);
#endif
		movers[i].first->Save();
	}
	result.moved = (int)movers.size();

	int misplaced = 0;
	std::map<uint32_t, int> finalCell;
	for (const rules::Placement& placement : plan.placements) {
		std::map<uint32_t, LPITEM>::iterator it = handleOf.find(placement.id);
		if (it == handleOf.end())
			continue;
		LPITEM item = it->second;
		if (item->GetOwner() != ch || item->GetCell() != (WORD)placement.cell ||
				ch->GetInventoryItem((WORD)placement.cell) != item) {
			++misplaced;
			Rescue(ch, item);
		}
		finalCell[placement.id] = item->GetOwner() == ch ? item->GetCell() : -1;
	}

	// Every row this changed, written now: the stacks that grew and the items
	// that moved (FlushRow).
	std::set<LPITEM> touched;
	for (const rules::Transfer& transfer : plan.transfers) {
		std::map<uint32_t, LPITEM>::iterator to = handleOf.find(transfer.to);
		if (to != handleOf.end())
			touched.insert(to->second);
	}
	for (size_t i = 0; i < movers.size(); ++i)
		touched.insert(movers[i].first);
	for (std::set<LPITEM>::const_iterator it = touched.begin(); it != touched.end(); ++it)
		if ((*it)->GetOwner() == ch)
			FlushRow(*it);

	// The quickslots: each one that pointed at a stack in the four pages now
	// points where that stack - or the stack it was poured into - stands. One
	// that pointed at an empty cell pointed at nothing, and is removed rather
	// than left to land on whatever stands there now.
	for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i) {
		const TQuickslot& old = before[i];
		if (old.type != QUICKSLOT_TYPE_ITEM || old.pos >= INVENTORY_DEFAULT_MAX_NUM)
			continue;
		TQuickslot* current = NULL;
		ch->GetQuickslot((BYTE)i, &current);
		std::map<int, uint32_t>::const_iterator at = idAtCell.find(old.pos);
		if (at == idAtCell.end()) {
			if (current && current->type == QUICKSLOT_TYPE_ITEM && current->pos == old.pos)
				ch->DelQuickslot((BYTE)i);
			continue;
		}
		uint32_t id = at->second;
		std::map<uint32_t, uint32_t>::const_iterator survivor = plan.survivorOf.find(id);
		if (survivor != plan.survivorOf.end())
			id = survivor->second;
		std::map<uint32_t, int>::const_iterator cell = finalCell.find(id);
		if (cell == finalCell.end() || cell->second < 0)
			continue;
		if (current && current->type == QUICKSLOT_TYPE_ITEM && current->pos == (WORD)cell->second)
			continue;
		TQuickslot slot;
		slot.type = QUICKSLOT_TYPE_ITEM;
		slot.pos = (WORD)cell->second;
		ch->SetQuickslot((BYTE)i, slot);
	}

	// The bag as the server holds it, said once more to the client and checked
	// against the engine's own grid.
	//
	// A move is a pair of packets - ITEM_DEL for the cell left, ITEM_SET for
	// the cell taken (CHARACTER::SetItem sends both) - so a client that read
	// every one of them needs nothing here. What the pairs cannot repair is a
	// client that missed one, and a bag laid out in one operation sends
	// dozens: "w te ktore staly sie puste w wyniku sortowania juz nie [moge
	// przeniesc] ... wystarczy przelogowac postac" (Dearminder, 19 September)
	// is exactly that shape, and a relog is what makes it go away, because a
	// relog is the server saying the whole bag again. So the operation ends by
	// saying it: the cells it touched, and only those, are restated from what
	// the server holds. Cheap - a click a player makes by hand, at most a few
	// dozen cells - and it cannot be wrong, because it is a copy of the truth.
	//
	// The grid is checked with it. bItemGrid is what GetEmptyInventory reads,
	// and a stale cell there is a cell no purchase, no pickup and no safebox
	// checkout can ever use again; SetItem maintains it, so this has nothing to
	// repair while that holds - and says so in syserr on the day it does not.
	if (ch->GetDesc()) {
		std::set<WORD> restate;
		for (size_t i = 0; i < movers.size(); ++i)
			restate.insert(movers[i].second);
		for (std::map<int, uint32_t>::const_iterator it = idAtCell.begin(); it != idAtCell.end(); ++it)
			if (it->first >= 0 && it->first < (int)INVENTORY_DEFAULT_MAX_NUM)
				restate.insert((WORD)it->first);
		for (std::set<WORD>::const_iterator it = restate.begin(); it != restate.end(); ++it)
			RestateCell(ch, *it);
	}
	// Counted before the plan as well as after it, and only a count that GREW
	// is this operation's doing and worth a line.
	//
	// It used to report any count at all, and the measurement says what that
	// was worth: on the test world on 20 September, 2285 runs over 1002
	// different bags, and not one of them ended with more than it began with -
	// 2285 syserr lines a day about a condition the sort did not cause and
	// cannot repair. What the line is for is the day the sort breaks a bag,
	// and that is what it says now.
	//
	// The pre-existing count is left undiagnosed on purpose rather than
	// quietly dropped: one to eight cells of most bags are cells this counter
	// calls empty and IsEmptyItemGrid calls taken, and nothing yet says which
	// of the two is wrong. Nobody has reported a bag that says it is full
	// while cells are visibly free, which is what the engine being right would
	// look like, so the counter is the likelier suspect - but that is a guess,
	// and a guess does not belong in syserr once a second.
	const int gridHoles = CountPlayerBotGridHoles(ch, bagCells);
	if (gridHoles > holesBefore)
		sys_err("INVENTORY_ARRANGE: pid=%u name=%s empty cells marked taken in the grid: %d before, %d after",
				ch->GetPlayerID(), ch->GetName(), holesBefore, gridHoles);

	std::map<DWORD, uint64_t> unitsAfter;
	for (WORD cell = 0; cell < INVENTORY_DEFAULT_MAX_NUM; ++cell) {
		LPITEM item = ch->GetInventoryItem(cell);
		if (item)
			unitsAfter[item->GetVnum()] += item->GetCount();
	}
	if (unitsAfter != unitsBefore)
		for (std::map<DWORD, uint64_t>::const_iterator it = unitsBefore.begin(); it != unitsBefore.end(); ++it) {
			std::map<DWORD, uint64_t>::const_iterator now = unitsAfter.find(it->first);
			const uint64_t after = now == unitsAfter.end() ? 0 : now->second;
			if (after != it->second)
				sys_err("INVENTORY_ARRANGE: pid=%u name=%s vnum %u held %llu units before and %llu after",
						ch->GetPlayerID(), ch->GetName(), it->first, (unsigned long long)it->second,
						(unsigned long long)after);
		}

	const unsigned int micros = (unsigned int)std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - started).count();
	result.micros = micros;
	if (misplaced)
		sys_err("INVENTORY_ARRANGE: pid=%u name=%s %d item(s) were not where the plan put them",
				ch->GetPlayerID(), ch->GetName(), misplaced);
	if (fromPlayer)
		sys_log(0, "INVENTORY_ARRANGE: pid=%u name=%s items=%d moved=%d merged=%d units=%u pinned=%d strategy=%d us=%u",
				ch->GetPlayerID(), ch->GetName(), result.items, result.moved, result.merged, result.units,
				result.pinned, result.strategy, micros);
	result.code = RESULT_DONE;
	return result;
}

// ---------------------------------------------------------------------------
// The safebox (blasty's proposal of 19 September, Tieru's yes the same
// minute): its own "Scal i uporzadkuj", and a stack moved by count across the
// safebox and the bag or inside the safebox. The client has no safebox packet
// that carries a count - SafeboxCheckin, SafeboxCheckout and SafeboxItemMove
// name cells only - so these are commands. The package's
// ENABLE_MT2009_DISABLE_SAFEBOX_STACK, which took stacking out of
// CSafebox::MoveItem, stays on: that stacking destroyed the item it had just
// failed to take out of the box whenever the owner was also browsing the item
// shop (CSafebox::Remove refuses then and answers NULL, and M2_DESTROY_ITEM
// went ahead regardless), which left a freed item in the box. Everything here
// asks for that state before the first change, and nothing changes after a
// refusal.
//
// A safebox item is written at once: the db core puts the SAFEBOX window
// straight into the table (QUERY_ITEM_SAVE caches every other window), and
// QUERY_SAFEBOX_LOAD reads the table. So a count changed in the box goes out
// with FlushDelayedSave, and whatever changed in the bag with FlushRow - the
// pair SafeboxCheckout sends - so the units are never in both places, or in
// neither, for the minutes the bag's cache would otherwise hold them.
namespace {

static_assert(SAFEBOX_PAGE_WIDTH == rules::PAGE_COLUMNS, "the safebox's page is the planner's width");
static_assert(SAFEBOX_PAGE_HEIGHT == rules::PAGE_ROWS, "the safebox's page is the planner's height");

const int SAFEBOX_PAGE_CELLS = SAFEBOX_PAGE_WIDTH * SAFEBOX_PAGE_HEIGHT;

std::unordered_map<DWORD, DWORD> s_mapLastSafeboxArrange;

enum EHands {
	HANDS_OK = 0,
	HANDS_BUSY,
	HANDS_DEAD,
	HANDS_NO_SAFEBOX,
};

// Whether the open safebox may be worked on now: its owner alive, in no quest,
// and busy with nothing but the safebox itself. CanHandleItem's default lets
// the item shop through (BUSY_CAN_HANDLE_ITEM_EXCLUDE) and CSafebox::Add and
// Remove do not, so it is asked here with only the safebox excluded.
int SafeboxHands(LPCHARACTER ch, CSafebox*& box)
{
	box = NULL;
	if (!ch || !ch->IsPC() || !ch->IsItemLoaded())
		return HANDS_BUSY;
	if (ch->IsDead())
		return HANDS_DEAD;
	if (quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID())->IsRunning())
		return HANDS_BUSY;
	if (!ch->CanHandleItem(false, false, BUSY_SAFEBOX))
		return HANDS_BUSY;
	box = ch->GetSafebox();
	if (!box || !ch->IsOpenSafebox() || !box->IsValidPosition(0))
		return HANDS_NO_SAFEBOX;
	return HANDS_OK;
}

int TransferCodeOf(int hands)
{
	switch (hands) {
	case HANDS_DEAD: return TRANSFER_DEAD;
	case HANDS_NO_SAFEBOX: return TRANSFER_NO_SAFEBOX;
	default: return TRANSFER_BUSY;
	}
}

// The pages the box has: whole pages its grid holds, never past SAFEBOX_MAX_NUM,
// which is all the cells CSafebox keeps pointers for.
int SafeboxPages(CSafebox* box)
{
	int pages = 0;
	while (pages < SAFEBOX_PAGE_COUNT && box->IsValidPosition((DWORD)((pages + 1) * SAFEBOX_PAGE_CELLS - 1)))
		++pages;
	return pages;
}

// The safebox item whose top cell `pos` is, or NULL. CSafebox::Get answers
// only for top cells; a cell another item covers is empty to it and full to
// IsEmpty.
LPITEM SafeboxItemAt(CSafebox* box, unsigned int pos)
{
	if (pos >= (unsigned int)SAFEBOX_MAX_NUM || !box->IsValidPosition(pos))
		return NULL;
	LPITEM item = box->Get(pos);
	if (!item || item->GetCell() != pos || item->GetWindow() != SAFEBOX)
		return NULL;
	return item;
}

bool IsSplittable(LPITEM item)
{
	return item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK) && item->GetMaxStack() > 1;
}

// The part cut off a stack: everything SameStack compares, so it pours back
// into its source later. MoveItem's own split copies the sockets alone.
LPITEM CutOff(LPITEM item, ITEM_COUNT units)
{
	LPITEM part = ITEM_MANAGER::instance().CreateItem(item->GetOriginalVnum(), units);
	if (!part)
		return NULL;
	part->SetSockets(item->GetSockets());
	part->SetAttributes(item->GetAttributes());
	return part;
}

// A count changed on an item in the box: the client told (Refresh; the
// ITEM_UPDATE SetCount sends for the SAFEBOX window is dropped by the client,
// whose IsValidItemPosition says no to it) and the row written now.
void RefreshSafeboxItem(CSafebox* box, LPITEM item)
{
	box->Refresh(item->GetCell(), true);
	ITEM_MANAGER::instance().FlushDelayedSave(item);
}

void LogSafebox(LPCHARACTER ch, LPITEM item, const char* how, unsigned int units, const char* way)
{
	char hint[128];
	snprintf(hint, sizeof(hint), "%s %u %s", item->GetName(), units, way);
	LogManager::instance().ItemLog(ch, item, how, hint);
}

// What SafeboxCheckin refuses about an item, and what it does not ask because
// its cells cannot hold such an item: worn, a dragon stone, in a trade.
bool MayGoIntoSafebox(LPCHARACTER ch, LPITEM item)
{
	if (item->IsEquipped() || item->IsDragonSoul() || item->IsExchanging() || item->isLocked())
		return false;
	if (item->GetVnum() == UNIQUE_ITEM_SAFEBOX_EXPAND || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_SAFEBOX))
		return false;
	// @fixme140, as the checkin has it.
	if (item->GetType() == ITEM_BELT && CBeltInventoryHelper::IsExistItemInBeltInventory(ch))
		return false;
	return true;
}

// An item the plan was sure of and the box did not take where it was told.
// Not expected - every place is checked first - but an ownerless item is
// destroyed by the next delayed save, so it goes into the first place in the
// box it fits, then into the bag, then to its owner's feet (Rescue).
void RescueIntoSafebox(LPCHARACTER ch, CSafebox* box, LPITEM item, int pages)
{
	if (item->GetOwner())
		return;
	for (int pos = 0; pos < pages * SAFEBOX_PAGE_CELLS; ++pos)
		if (box->IsEmpty(pos, item->GetSize()) && box->Add(pos, item)) {
			sys_err("SAFEBOX_ARRANGE: pid=%u name=%s item %u (%s) put back at safebox cell %d",
					ch->GetPlayerID(), ch->GetName(), item->GetID(), item->GetName(), pos);
			return;
		}
	Rescue(ch, item);
}

void CountSafeboxUnits(CSafebox* box, int pages, std::map<DWORD, uint64_t>& units)
{
	for (int pos = 0; pos < pages * SAFEBOX_PAGE_CELLS; ++pos) {
		LPITEM item = box->Get(pos);
		if (item)
			units[item->GetVnum()] += item->GetCount();
	}
}

}  // namespace

TResult ArrangeSafebox(LPCHARACTER ch, bool fromPlayer)
{
	TResult result;
	CSafebox* box = NULL;
	const int hands = SafeboxHands(ch, box);
	if (hands != HANDS_OK) {
		result.code = hands == HANDS_DEAD ? RESULT_DEAD : (hands == HANDS_NO_SAFEBOX ? RESULT_NO_SAFEBOX : RESULT_BUSY);
		return result;
	}
	const DWORD now = get_dword_time();
	if (fromPlayer) {
		std::unordered_map<DWORD, DWORD>::iterator last = s_mapLastSafeboxArrange.find(ch->GetPlayerID());
		if (last != s_mapLastSafeboxArrange.end() && now - last->second < PLAYER_COOLDOWN_MS) {
			result.code = RESULT_COOLDOWN;
			return result;
		}
		if (s_mapLastSafeboxArrange.size() > 4096)
			for (std::unordered_map<DWORD, DWORD>::iterator it = s_mapLastSafeboxArrange.begin(); it != s_mapLastSafeboxArrange.end();)
				it = now - it->second > PLAYER_COOLDOWN_MS ? s_mapLastSafeboxArrange.erase(it) : std::next(it);
		s_mapLastSafeboxArrange[ch->GetPlayerID()] = now;
	}
	const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
	const int pages = SafeboxPages(box);
	if (pages < 1) {
		result.code = RESULT_NO_SAFEBOX;
		return result;
	}

	std::vector<rules::Item> items;
	std::vector<LPITEM> handles;
	std::map<uint32_t, LPITEM> handleOf;
	for (int pos = 0; pos < pages * SAFEBOX_PAGE_CELLS; ++pos) {
		LPITEM item = box->Get(pos);
		if (!item)
			continue;
		if (item->GetCell() != (WORD)pos || item->GetOwner() != ch || item->GetWindow() != SAFEBOX ||
				item->GetID() == 0 || handleOf.count(item->GetID())) {
			sys_err("SAFEBOX_ARRANGE: pid=%u name=%s refused: item %u (%s) at safebox cell %d says cell %u window %u",
					ch->GetPlayerID(), ch->GetName(), item->GetID(), item->GetName(), pos,
					item->GetCell(), item->GetWindow());
			result.code = RESULT_INCONSISTENT;
			return result;
		}
		const int size = item->GetSize();
		if (size > rules::MAX_HEIGHT) {
			sys_err("SAFEBOX_ARRANGE: pid=%u name=%s refused: item %u (%s) is %d cells tall",
					ch->GetPlayerID(), ch->GetName(), item->GetID(), item->GetName(), size);
			result.code = RESULT_INCONSISTENT;
			return result;
		}
		rules::Item planned;
		planned.id = item->GetID();
		planned.cell = pos;
		planned.height = size >= 1 ? size : 1;
		planned.count = item->GetCount();
		planned.maxStack = item->GetMaxStack();
		planned.pinned = item->isLocked() || item->IsExchanging() || planned.height != size;
		SortKeyOf(item, planned.key);
		items.push_back(planned);
		handles.push_back(item);
		handleOf[planned.id] = item;
		if (planned.pinned)
			++result.pinned;
	}
	result.items = (int)items.size();
	std::map<DWORD, uint64_t> unitsBefore;
	CountSafeboxUnits(box, pages, unitsBefore);

	std::vector<LPITEM> representatives;
	for (size_t i = 0; i < items.size(); ++i) {
		LPITEM item = handles[i];
		if (items[i].pinned || !IsSplittable(item))
			continue;
		size_t group = 0;
		for (; group < representatives.size(); ++group)
			if (SameStack(representatives[group], item))
				break;
		if (group == representatives.size())
			representatives.push_back(item);
		items[i].mergeGroup = (uint32_t)group + 1;
	}

	// The box's own rule for what it holds (rules::ValidGrid): an item may
	// stand across a page edge, and the plan lays it out inside a page.
	const rules::Plan plan = rules::MakePlan(items, pages, false);
	if (!plan.ok) {
		sys_err("SAFEBOX_ARRANGE: pid=%u name=%s refused: the safebox is not a legal layout (%d items, %d pages)",
				ch->GetPlayerID(), ch->GetName(), result.items, pages);
		result.code = RESULT_INCONSISTENT;
		return result;
	}
	result.strategy = plan.strategy;
	if (plan.transfers.empty() && plan.moved == 0) {
		result.code = RESULT_NOTHING;
		return result;
	}

	// The pours. The receiver grows first; an emptied giver leaves the box the
	// way CSafebox's own stacking took one out, and is destroyed - SetCount(0)
	// would only clear its owner, since RemoveFromCharacter leaves the box's
	// pointer and grid alone for the SAFEBOX window.
	std::set<LPITEM> recounted;
	for (const rules::Transfer& transfer : plan.transfers) {
		std::map<uint32_t, LPITEM>::iterator from = handleOf.find(transfer.from);
		std::map<uint32_t, LPITEM>::iterator to = handleOf.find(transfer.to);
		if (from == handleOf.end() || to == handleOf.end() || from->second->GetCount() < transfer.units ||
				to->second->GetCount() + transfer.units > to->second->GetMaxStack()) {
			sys_err("SAFEBOX_ARRANGE: pid=%u name=%s pour %u -> %u of %u refused; the safebox stays as poured so far",
					ch->GetPlayerID(), ch->GetName(), transfer.from, transfer.to, transfer.units);
			for (std::set<LPITEM>::const_iterator it = recounted.begin(); it != recounted.end(); ++it)
				RefreshSafeboxItem(box, *it);
			result.code = RESULT_INCONSISTENT;
			return result;
		}
		LPITEM giver = from->second;
		to->second->SetCount(to->second->GetCount() + transfer.units);
		recounted.insert(to->second);
		const ITEM_COUNT left = giver->GetCount() - transfer.units;
		if (left == 0) {
			recounted.erase(giver);
			handleOf.erase(from);
			box->Remove(giver->GetCell());
			M2_DESTROY_ITEM(giver);
			++result.merged;
		} else {
			giver->SetCount(left);
			recounted.insert(giver);
		}
		result.units += transfer.units;
	}

	// The moves: out of the box first, then each into its new cell, so a cycle
	// needs no free cell in between. Add writes the row (Save and
	// FlushDelayedSave) and tells the client.
	std::vector<std::pair<LPITEM, int> > movers;
	for (const rules::Placement& placement : plan.placements) {
		std::map<uint32_t, LPITEM>::iterator it = handleOf.find(placement.id);
		if (it == handleOf.end())
			continue;
		if (it->second->GetCell() != (WORD)placement.cell)
			movers.push_back(std::make_pair(it->second, placement.cell));
	}
	for (size_t i = 0; i < movers.size(); ++i)
		box->Remove(movers[i].first->GetCell());
	for (size_t i = 0; i < movers.size(); ++i) {
		LPITEM item = movers[i].first;
		recounted.erase(item);
		if (!box->IsEmpty(movers[i].second, item->GetSize()) || !box->Add(movers[i].second, item))
			RescueIntoSafebox(ch, box, item, pages);
	}
	result.moved = (int)movers.size();
	// What was poured and did not move: its new count to the client and the
	// table.
	for (std::set<LPITEM>::const_iterator it = recounted.begin(); it != recounted.end(); ++it)
		RefreshSafeboxItem(box, *it);

	int misplaced = 0;
	for (const rules::Placement& placement : plan.placements) {
		std::map<uint32_t, LPITEM>::iterator it = handleOf.find(placement.id);
		if (it == handleOf.end())
			continue;
		LPITEM item = it->second;
		if (item->GetOwner() != ch || item->GetWindow() != SAFEBOX || item->GetCell() != (WORD)placement.cell ||
				box->Get(placement.cell) != item)
			++misplaced;
	}
	std::map<DWORD, uint64_t> unitsAfter;
	CountSafeboxUnits(box, pages, unitsAfter);
	if (unitsAfter != unitsBefore)
		for (std::map<DWORD, uint64_t>::const_iterator it = unitsBefore.begin(); it != unitsBefore.end(); ++it) {
			std::map<DWORD, uint64_t>::const_iterator after = unitsAfter.find(it->first);
			const uint64_t held = after == unitsAfter.end() ? 0 : after->second;
			if (held != it->second)
				sys_err("SAFEBOX_ARRANGE: pid=%u name=%s vnum %u held %llu units before and %llu after",
						ch->GetPlayerID(), ch->GetName(), it->first, (unsigned long long)it->second,
						(unsigned long long)held);
		}
	const unsigned int micros = (unsigned int)std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - started).count();
	result.micros = micros;
	if (misplaced)
		sys_err("SAFEBOX_ARRANGE: pid=%u name=%s %d item(s) were not where the plan put them",
				ch->GetPlayerID(), ch->GetName(), misplaced);
	if (fromPlayer)
		sys_log(0, "SAFEBOX_ARRANGE: pid=%u name=%s pages=%d items=%d moved=%d merged=%d units=%u pinned=%d strategy=%d us=%u",
				ch->GetPlayerID(), ch->GetName(), pages, result.items, result.moved, result.merged, result.units,
				result.pinned, result.strategy, micros);
	result.code = RESULT_DONE;
	return result;
}

TTransfer PutIntoSafebox(LPCHARACTER ch, unsigned int bagCell, unsigned int safePos, unsigned int count)
{
	TTransfer result;
	CSafebox* box = NULL;
	const int hands = SafeboxHands(ch, box);
	if (hands != HANDS_OK) {
		result.code = TransferCodeOf(hands);
		return result;
	}
	if (bagCell >= (unsigned int)INVENTORY_DEFAULT_MAX_NUM || safePos >= (unsigned int)SAFEBOX_MAX_NUM ||
			!box->IsValidPosition(safePos)) {
		result.code = TRANSFER_BAD_REQUEST;
		return result;
	}
	if (!PulseManager::Instance().IncreaseClock(ch->GetPlayerID(), ePulse::SafeboxCheckInOut, std::chrono::milliseconds(250))) {
		result.code = TRANSFER_COOLDOWN;
		return result;
	}
	LPITEM item = ch->GetInventoryItem((WORD)bagCell);
	if (!item || item->GetCell() != bagCell || item->GetWindow() != INVENTORY || item->GetOwner() != ch) {
		result.code = TRANSFER_NO_ITEM;
		return result;
	}
	if (!MayGoIntoSafebox(ch, item)) {
		result.code = TRANSFER_REFUSED;
		return result;
	}
	LPITEM held = box->Get(safePos);
	const bool empty = !held && box->IsEmpty(safePos, item->GetSize());
	const bool same = held && held->GetCell() == safePos && SameStack(held, item);
	const rules::TransferPlan plan = rules::PlanTransfer(item->GetCount(), count, IsSplittable(item), empty, same,
			held ? held->GetCount() : 0, held ? held->GetMaxStack() : 0);
	switch (plan.kind) {
	case rules::TRANSFER_KIND_MOVE:
		// SafeboxCheckin's own steps.
		item->RemoveFromCharacter();
		ch->SyncQuickslot(QUICKSLOT_TYPE_ITEM, (WORD)bagCell, 255);
		if (!box->Add(safePos, item)) {
			item->AddToCharacter(ch, TItemPos(INVENTORY, (WORD)bagCell));
			result.code = TRANSFER_REFUSED;
			return result;
		}
		LogSafebox(ch, item, "SAFEBOX PUT", plan.units, "");
		break;
	case rules::TRANSFER_KIND_SPLIT: {
		LPITEM part = CutOff(item, plan.units);
		if (!part) {
			result.code = TRANSFER_REFUSED;
			return result;
		}
		// Into the box first: nothing has left the bag if the box says no.
		if (!box->Add(safePos, part)) {
			M2_DESTROY_ITEM(part);
			result.code = TRANSFER_REFUSED;
			return result;
		}
		item->SetCount(item->GetCount() - plan.units);
		FlushRow(item);
		LogSafebox(ch, part, "SAFEBOX PUT", plan.units, "split");
		break;
	}
	case rules::TRANSFER_KIND_POUR:
		held->SetCount(held->GetCount() + plan.units);
		RefreshSafeboxItem(box, held);
		LogSafebox(ch, held, "SAFEBOX PUT", plan.units, "stack");
		if (plan.sourceEmptied) {
			item->SetCount(0);  // MoveItem's stacking: the quickslot follows, the row is deleted
		} else {
			item->SetCount(item->GetCount() - plan.units);
			FlushRow(item);
		}
		break;
	default:
		result.code = plan.refusal == rules::TRANSFER_REFUSED_FULL ? TRANSFER_FULL :
				(plan.refusal == rules::TRANSFER_REFUSED_EMPTY ? TRANSFER_NO_ITEM : TRANSFER_OCCUPIED);
		return result;
	}
	result.code = TRANSFER_DONE;
	result.units = plan.units;
	return result;
}

TTransfer TakeFromSafebox(LPCHARACTER ch, unsigned int safePos, unsigned int bagCell, unsigned int count)
{
	TTransfer result;
	CSafebox* box = NULL;
	const int hands = SafeboxHands(ch, box);
	if (hands != HANDS_OK) {
		result.code = TransferCodeOf(hands);
		return result;
	}
	if (bagCell >= (unsigned int)INVENTORY_DEFAULT_MAX_NUM || safePos >= (unsigned int)SAFEBOX_MAX_NUM) {
		result.code = TRANSFER_BAD_REQUEST;
		return result;
	}
	// SafeboxCheckout's two switches.
	if (quest::CQuestManager::instance().GetEventFlag("block_safebox") > 0 || g_bChannel > 90) {
		result.code = TRANSFER_REFUSED;
		return result;
	}
	if (!PulseManager::Instance().IncreaseClock(ch->GetPlayerID(), ePulse::SafeboxCheckInOut, std::chrono::milliseconds(250))) {
		result.code = TRANSFER_COOLDOWN;
		return result;
	}
	LPITEM held = SafeboxItemAt(box, safePos);
	if (!held || held->GetOwner() != ch) {
		result.code = TRANSFER_NO_ITEM;
		return result;
	}
	// A dragon stone goes to its own window, which the engine's packet knows.
	if (held->IsDragonSoul() || held->IsExchanging()) {
		result.code = TRANSFER_REFUSED;
		return result;
	}
	const TItemPos pos(INVENTORY, (WORD)bagCell);
	LPITEM there = ch->GetInventoryItem((WORD)bagCell);
	const bool empty = !there && ch->IsEmptyItemGrid(pos, held->GetSize());
	const bool same = there && there->GetCell() == bagCell && !there->isLocked() && !there->IsExchanging() &&
			SameStack(there, held);
	const rules::TransferPlan plan = rules::PlanTransfer(held->GetCount(), count, IsSplittable(held), empty, same,
			there ? there->GetCount() : 0, there ? there->GetMaxStack() : 0);
	switch (plan.kind) {
	case rules::TRANSFER_KIND_MOVE:
		// SafeboxCheckout's own steps, and its HEADER_GD_ITEM_FLUSH.
		box->Remove(safePos);
		if (!held->AddToCharacter(ch, pos)) {
			box->Add(safePos, held);
			result.code = TRANSFER_REFUSED;
			return result;
		}
		FlushRow(held);
		LogSafebox(ch, held, "SAFEBOX GET", plan.units, "");
		break;
	case rules::TRANSFER_KIND_SPLIT: {
		LPITEM part = CutOff(held, plan.units);
		if (!part) {
			result.code = TRANSFER_REFUSED;
			return result;
		}
		if (!part->AddToCharacter(ch, pos)) {
			M2_DESTROY_ITEM(part);
			result.code = TRANSFER_REFUSED;
			return result;
		}
		FlushRow(part);
		held->SetCount(held->GetCount() - plan.units);
		RefreshSafeboxItem(box, held);
		LogSafebox(ch, part, "SAFEBOX GET", plan.units, "split");
		break;
	}
	case rules::TRANSFER_KIND_POUR:
		there->SetCount(there->GetCount() + plan.units);
		FlushRow(there);
		LogSafebox(ch, there, "SAFEBOX GET", plan.units, "stack");
		if (plan.sourceEmptied) {
			box->Remove(safePos);
			M2_DESTROY_ITEM(held);
		} else {
			held->SetCount(held->GetCount() - plan.units);
			RefreshSafeboxItem(box, held);
		}
		break;
	default:
		result.code = plan.refusal == rules::TRANSFER_REFUSED_FULL ? TRANSFER_FULL :
				(plan.refusal == rules::TRANSFER_REFUSED_EMPTY ? TRANSFER_NO_ITEM : TRANSFER_OCCUPIED);
		return result;
	}
	result.code = TRANSFER_DONE;
	result.units = plan.units;
	return result;
}

TTransfer MoveInSafebox(LPCHARACTER ch, unsigned int fromPos, unsigned int toPos, unsigned int count)
{
	TTransfer result;
	CSafebox* box = NULL;
	const int hands = SafeboxHands(ch, box);
	if (hands != HANDS_OK) {
		result.code = TransferCodeOf(hands);
		return result;
	}
	if (fromPos == toPos || fromPos >= (unsigned int)SAFEBOX_MAX_NUM || toPos >= (unsigned int)SAFEBOX_MAX_NUM ||
			!box->IsValidPosition(toPos)) {
		result.code = TRANSFER_BAD_REQUEST;
		return result;
	}
	if (!PulseManager::Instance().IncreaseCount(ch->GetPlayerID(), ePulse::SafeboxMove, std::chrono::milliseconds(500), 5)) {
		result.code = TRANSFER_COOLDOWN;
		return result;
	}
	LPITEM held = SafeboxItemAt(box, fromPos);
	if (!held || held->GetOwner() != ch) {
		result.code = TRANSFER_NO_ITEM;
		return result;
	}
	if (held->IsExchanging()) {
		result.code = TRANSFER_REFUSED;
		return result;
	}
	LPITEM there = box->Get(toPos);
	const bool same = there && there != held && there->GetCell() == toPos && SameStack(there, held);
	// A whole stack onto a place no stack stands on goes the packet's own way,
	// CSafebox::MoveItem, which says no when the place is not clear.
	const bool whole = !IsSplittable(held) || count == 0 || count >= held->GetCount();
	if (!there && whole) {
		if (!box->MoveItem((BYTE)fromPos, (BYTE)toPos)) {
			result.code = TRANSFER_OCCUPIED;
			return result;
		}
		result.code = TRANSFER_DONE;
		result.units = held->GetCount();
		return result;
	}
	const bool empty = !there && box->IsEmpty(toPos, held->GetSize());
	const rules::TransferPlan plan = rules::PlanTransfer(held->GetCount(), count, IsSplittable(held), empty, same,
			there ? there->GetCount() : 0, there ? there->GetMaxStack() : 0);
	switch (plan.kind) {
	case rules::TRANSFER_KIND_SPLIT: {
		LPITEM part = CutOff(held, plan.units);
		if (!part) {
			result.code = TRANSFER_REFUSED;
			return result;
		}
		if (!box->Add(toPos, part)) {
			M2_DESTROY_ITEM(part);
			result.code = TRANSFER_REFUSED;
			return result;
		}
		held->SetCount(held->GetCount() - plan.units);
		RefreshSafeboxItem(box, held);
		LogSafebox(ch, part, "SAFEBOX MOVE", plan.units, "split");
		break;
	}
	case rules::TRANSFER_KIND_POUR:
		there->SetCount(there->GetCount() + plan.units);
		RefreshSafeboxItem(box, there);
		LogSafebox(ch, there, "SAFEBOX MOVE", plan.units, "stack");
		if (plan.sourceEmptied) {
			box->Remove(fromPos);
			M2_DESTROY_ITEM(held);
		} else {
			held->SetCount(held->GetCount() - plan.units);
			RefreshSafeboxItem(box, held);
		}
		break;
	default:
		// A whole stack onto a free place went to MoveItem above; what is left
		// is a refusal.
		result.code = plan.refusal == rules::TRANSFER_REFUSED_FULL ? TRANSFER_FULL :
				(plan.refusal == rules::TRANSFER_REFUSED_EMPTY ? TRANSFER_NO_ITEM : TRANSFER_OCCUPIED);
		return result;
	}
	result.code = TRANSFER_DONE;
	result.units = plan.units;
	return result;
}

}  // namespace playerbot_arrange

#else  // r40250: two pages and no horse page; the bots keep their own tidy pass there.

namespace playerbot_arrange {

TResult ArrangeInventory(LPCHARACTER, bool)
{
	TResult result;
	result.code = RESULT_UNSUPPORTED;
	return result;
}

TResult ArrangeSafebox(LPCHARACTER, bool)
{
	TResult result;
	result.code = RESULT_UNSUPPORTED;
	return result;
}

TTransfer PutIntoSafebox(LPCHARACTER, unsigned int, unsigned int, unsigned int)
{
	return TTransfer();
}

TTransfer TakeFromSafebox(LPCHARACTER, unsigned int, unsigned int, unsigned int)
{
	return TTransfer();
}

TTransfer MoveInSafebox(LPCHARACTER, unsigned int, unsigned int, unsigned int)
{
	return TTransfer();
}

}  // namespace playerbot_arrange

#endif
