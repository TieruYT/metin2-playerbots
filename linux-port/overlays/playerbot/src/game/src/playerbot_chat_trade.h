#ifndef __INC_METIN2_PLAYERBOT_CHAT_TRADE_H__
#define __INC_METIN2_PLAYERBOT_CHAT_TRADE_H__

// Trading over the chat: what a bot shouts about its counter and its wants,
// and what it whispers back when a player shouts "Kupie ..." or "Sprzedam ...".
//
// The market already exists - counters in Joan and Bokjung, a ledger of who
// is short of what - but a player only found out by walking the ring. A
// player on a real server finds out from the shout channel, and answers a
// shout with a whisper, and that is the shape copied here: a bot that opens
// a counter with something worth crossing town for says so once, a bot that
// walked the market for a material and found none asks for it once, and a
// player's own shout is read for the two words that matter and answered by
// the one bot best placed to answer - the nearest counter that has the
// thing, or a bot that is short of it.
//
// The engine side of this is patch 0007: CInputMain::Chat hands a player's
// shout to CPlayerBotManager::OnPlayerShout after it has gone out, and
// CInputMain::Whisper hands a whisper addressed to a bot to OnPlayerWhisper
// instead of writing it to a descriptor with no client behind it. Both are
// one call each; everything they call is here.
//
// Text is CP1250, which is what the Polish client sends and what the item
// names in the proto are written in. Matching folds both sides to lowercase
// ASCII so "Kupię Kość Niedźwiedzia" finds "Kość Niedźwiedzia" whether or not
// the player bothered with the diacritics. What a bot says is ASCII, as
// everywhere else; the item names it quotes are the proto's own.
//
// An implementation fragment in the sense playerbot_types.h describes:
// include it exactly once, after playerbot_market.h - it reads the counters
// the way a shopping bot does, and the market's own helpers for what a bot
// wants.

// The players' names for items (FMS, 12D, bodzio ...) - pure, playerbot_conv_aliases.h.
#include "playerbot_conv_aliases.h"

namespace
{
	// One trade shout on the world channel this often, whoever it is from,
	// and one from any single bot this often. The refine announcements run at
	// one every three minutes; with these the channel carries a line a minute
	// at the most, which reads as a market and not as a wall.
	const DWORD PLAYERBOT_TRADE_SHOUT_INTERVAL = 90000;
	const DWORD PLAYERBOT_TRADE_SHOUT_BOT_INTERVAL = 1200000;
	// A player gets one whispered answer this often, so a shout repeated
	// twice does not bring two bots to the same door.
	const DWORD PLAYERBOT_TRADE_REPLY_INTERVAL = 8000;
	// Fewer letters than this after the verb is not a thing anybody meant.
	const size_t PLAYERBOT_TRADE_QUERY_MIN = 3;
	// The skill books the proto names one skill each - "Instr. Aura Miecza",
	// value 0 the skill - which is the only place the server has a skill's
	// Polish name. skill_proto holds the Korean ones.
	const DWORD PLAYERBOT_TRADE_SKILL_BOOK_FIRST = 50401;
	const DWORD PLAYERBOT_TRADE_SKILL_BOOK_LAST = 50511;

	DWORD s_dwPlayerBotTradeShoutTime = 0;
	std::map<DWORD, DWORD> s_mapPlayerBotTradeShoutTime;
	std::map<DWORD, DWORD> s_mapPlayerBotTradeReplyTime;

	const char* GetPlayerBotTownName(long mapIndex)
	{
		// The engine's own quests name these: new_quest_lv52 for the first
		// villages, new_quest_lv7 for the second.
		switch (mapIndex)
		{
			case 1: return "Yongan";
			case 3: return "Jayang";
			case PLAYERBOT_MAP_CHUNJO_M1: return "Joan";
			case PLAYERBOT_MAP_CHUNJO_M2: return "Bokjung";
			case 41: return "Pyongmoo";
			case 43: return "Bakra";
			default: return "miescie";
		}
	}

	// Lowercase ASCII from CP1250: the Polish letters go to their base, the
	// rest of the high half to '?', so a name compares the same however it
	// was typed.
	void FoldPlayerBotChatText(const char* in, char* out, size_t size)
	{
		size_t o = 0;
		for (const unsigned char* p = (const unsigned char*)(in ? in : ""); *p && o + 1 < size; ++p)
		{
			unsigned char c = *p;
			switch (c)
			{
				case 0xA5: case 0xB9: c = 'a'; break;
				case 0xC6: case 0xE6: c = 'c'; break;
				case 0xCA: case 0xEA: c = 'e'; break;
				case 0xA3: case 0xB3: c = 'l'; break;
				case 0xD1: case 0xF1: c = 'n'; break;
				case 0xD3: case 0xF3: c = 'o'; break;
				case 0x8C: case 0x9C: c = 's'; break;
				case 0x8F: case 0x9F: case 0xAF: case 0xBF: c = 'z'; break;
				default:
					if (c >= 'A' && c <= 'Z')
						c = (unsigned char)(c - 'A' + 'a');
					else if (c >= 0x80)
						c = '?';
					break;
			}
			out[o++] = (char)c;
		}
		out[o] = 0;
	}

	bool IsPlayerBotChatSeparator(char c)
	{
		return playerbot_lure_rules::IsSeparator(c);
	}

	// The whisper the client shows as one from the bot: the same packet
	// CInputMain::Whisper builds for a player, with the bot's name as sender.
	void SendPlayerBotWhisper(LPCHARACTER bot, LPCHARACTER to, const char* text)
	{
		if (!bot || !to || !to->GetDesc() || !text || !*text)
			return;
		const size_t len = std::min<size_t>(strlen(text), CHAT_MAX_LEN);
		TPacketGCWhisper pack;
		pack.bHeader = HEADER_GC_WHISPER;
		pack.bType = WHISPER_TYPE_NORMAL;
		pack.wSize = (WORD)(sizeof(TPacketGCWhisper) + len);
		strlcpy(pack.szNameFrom, bot->GetName(), sizeof(pack.szNameFrom));
		TEMP_BUFFER tmpbuf;
		tmpbuf.write(&pack, sizeof(pack));
		tmpbuf.write(text, (int)len);
		to->GetDesc()->Packet(tmpbuf.read_peek(), tmpbuf.size());
		sys_log(0, "PLAYERBOT_TRADE: whisper pid=%u name=%s to=%s text=\"%s\"",
				bot->GetPlayerID(), bot->GetName(), to->GetName(), text);
	}

	// A line on the world channel in the bot's name, within the two throttles.
	bool ShoutPlayerBotTrade(LPCHARACTER bot, const char* text, DWORD dwNow)
	{
		if (!bot || !text || !*text)
			return false;
		if (s_dwPlayerBotTradeShoutTime != 0 &&
				dwNow - s_dwPlayerBotTradeShoutTime < PLAYERBOT_TRADE_SHOUT_INTERVAL)
			return false;
		DWORD& last = s_mapPlayerBotTradeShoutTime[bot->GetPlayerID()];
		if (last != 0 && dwNow - last < PLAYERBOT_TRADE_SHOUT_BOT_INTERVAL)
			return false;
		s_dwPlayerBotTradeShoutTime = last = dwNow;
		char msg[CHAT_MAX_LEN + 1];
		snprintf(msg, sizeof(msg), "%s : %s", bot->GetName(), text);
		SendShout(msg, bot->GetEmpire());
		sys_log(0, "PLAYERBOT_TRADE: shout pid=%u name=%s text=\"%s\"",
				bot->GetPlayerID(), bot->GetName(), text);
		return true;
	}

	// The counter just opened with something worth crossing town for; the
	// keeper says so. Called from the stall code with the headline item.
	void AnnouncePlayerBotStall(LPCHARACTER ch, const char* pszItemName)
	{
		if (!ch || !pszItemName || !*pszItemName)
			return;
		char text[CHAT_MAX_LEN + 1];
		snprintf(text, sizeof(text), "Sprzedam %s - stragan w %s",
				pszItemName, GetPlayerBotTownName(ch->GetMapIndex()));
		ShoutPlayerBotTrade(ch, text, get_dword_time());
	}

	// The bot walked the market for a material and found none: it asks. Called
	// from the market code when a trip ends with nothing on offer.
	void AnnouncePlayerBotNeed(LPCHARACTER ch)
	{
		if (!ch)
			return;
		// On the 2.x line this is asked after every empty look at a first
		// village's stands, hundreds of times a minute, and only one shout in
		// PLAYERBOT_TRADE_SHOUT_INTERVAL goes out: the world-wide throttle is
		// asked before the bag is, which is the costly half.
		if (s_dwPlayerBotTradeShoutTime != 0 &&
				get_dword_time() - s_dwPlayerBotTradeShoutTime < PLAYERBOT_TRADE_SHOUT_INTERVAL)
			return;
		std::set<DWORD> wanted;
		CollectPlayerBotWantedMaterials(ch, wanted);
		if (wanted.empty())
			return;
		const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*wanted.begin());
		if (!proto)
			return;
		char text[CHAT_MAX_LEN + 1];
		snprintf(text, sizeof(text), "Kupie %s - kto ma, niech wystawi w %s",
				proto->szLocaleName, GetPlayerBotTownName(ch->GetMapIndex()));
		ShoutPlayerBotTrade(ch, text, get_dword_time());
	}

	// The skill a folded name means, from the per-skill books' names.
	DWORD FindPlayerBotSkillByName(const char* foldedQuery)
	{
		if (!foldedQuery || strlen(foldedQuery) < PLAYERBOT_TRADE_QUERY_MIN)
			return 0;
		for (DWORD vnum = PLAYERBOT_TRADE_SKILL_BOOK_FIRST; vnum <= PLAYERBOT_TRADE_SKILL_BOOK_LAST; ++vnum)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
			if (!proto || proto->bType != ITEM_SKILLBOOK)
				continue;
			char name[64];
			FoldPlayerBotChatText(proto->szLocaleName, name, sizeof(name));
			const char* p = name;
			if (strncmp(p, "instr. ", 7) == 0)
				p += 7;
			if (strstr(p, foldedQuery) || strstr(foldedQuery, p))
				return (DWORD)proto->alValues[0];
		}
		return 0;
	}

	// The Polish name of a skill, for a bot's own line about it.
	const char* GetPlayerBotSkillName(DWORD skillVnum)
	{
		for (DWORD vnum = PLAYERBOT_TRADE_SKILL_BOOK_FIRST; vnum <= PLAYERBOT_TRADE_SKILL_BOOK_LAST; ++vnum)
		{
			const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
			if (proto && proto->bType == ITEM_SKILLBOOK && (DWORD)proto->alValues[0] == skillVnum)
				return strncmp(proto->szLocaleName, "Instr. ", 7) == 0
						? proto->szLocaleName + 7 : proto->szLocaleName;
		}
		return "?";
	}

	bool PlayerBotItemNameMatches(LPITEM item, const char* foldedQuery)
	{
		if (!item || !item->GetProto())
			return false;
		char name[64];
		FoldPlayerBotChatText(item->GetProto()->szLocaleName, name, sizeof(name));
		return strstr(name, foldedQuery) != NULL;
	}

	// ------------------------------------------------------------ the stall
	//
	// What a bot has up for sale, whichever engine holds it: the classic stall
	// (CHARACTER::GetMyShop, the lines in vecShopOffers) or, on mt2009 with
	// ENABLE_IKASHOP_RENEWAL, the Ikarus offline shop the classic one is moved
	// to the moment it opens (ManagePlayerBotShopLifetime, "migrate_offline").
	// Asking only GetMyShop() on that engine says "no stall" for every keeper -
	// which is what the conversation and the shout answers did.
	struct TPlayerBotStallLine
	{
		std::string name;
		DWORD vnum;
		DWORD skill;
		long long price;
		unsigned int count;
		TPlayerBotStallLine() : vnum(0), skill(0), price(0), count(1) {}
	};

	struct TPlayerBotStall
	{
		bool open;
		bool offline;
		long mapIndex;
		long x;
		long y;
		int channel;
		std::vector<TPlayerBotStallLine> lines;
		TPlayerBotStall() : open(false), offline(false), mapIndex(0), x(0), y(0), channel(0) {}
	};

	std::string GetPlayerBotStallLineName(const TItemTable* proto, DWORD skill)
	{
		if (skill)
		{
			const char* skillName = GetPlayerBotSkillName(skill);
			if (skillName && strcmp(skillName, "?") != 0)
				return std::string("Instr. ") + skillName;
		}
		return proto ? std::string(proto->szLocaleName) : std::string();
	}

	// `keeper` may be NULL (the offline shop stands whether or not its owner
	// is in the world).
	bool GetPlayerBotStall(DWORD pid, LPCHARACTER keeper, TPlayerBotStall& out)
	{
		out = TPlayerBotStall();
		if (keeper && keeper->GetMyShop())
		{
			out.open = true;
			out.mapIndex = keeper->GetMapIndex();
			out.x = keeper->GetX();
			out.y = keeper->GetY();
			out.channel = g_bChannel;
			TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(pid);
			if (it != s_mapPlayerBotAIStates.end())
			{
				for (size_t i = 0; i < it->second.vecShopOffers.size(); ++i)
				{
					const TPlayerBotShopOffer& offer = it->second.vecShopOffers[i];
					LPITEM item = FindPlayerBotOfferItem(keeper, offer);
					if (!item || !item->GetProto())
						continue;
					TPlayerBotStallLine line;
					line.vnum = item->GetVnum();
					line.skill = GetPlayerBotSkillBookSkillVnum(item);
					line.name = GetPlayerBotStallLineName(item->GetProto(), line.skill);
					line.price = (long long)offer.dwPrice;
					line.count = offer.wCount ? offer.wCount : 1;
					out.lines.push_back(line);
				}
			}
			return true;
		}
#if defined(PLAYERBOT_ENGINE_MT2009) && defined(ENABLE_IKASHOP_RENEWAL)
		auto shop = ikashop::GetManager().GetShopByOwnerID(pid);
		if (shop && shop->GetDuration() > 0)
		{
			out.open = true;
			out.offline = true;
			out.mapIndex = shop->GetSpawn().map;
			out.x = shop->GetSpawn().x;
			out.y = shop->GetSpawn().y;
			out.channel = shop->GetSpawn().channel;
			for (const auto& entry : shop->GetItems())
			{
				const auto& shopItem = entry.second;
				if (!shopItem)
					continue;
				const TItemTable* proto = shopItem->GetTable();
				if (!proto)
					continue;
				TPlayerBotStallLine line;
				line.vnum = shopItem->GetInfo().vnum;
				if (proto->bType == ITEM_SKILLBOOK)
					line.skill = line.vnum == 50300 ? (DWORD)shopItem->GetInfo().alSockets[0] : (DWORD)proto->alValues[0];
				line.name = GetPlayerBotStallLineName(proto, line.skill);
				line.price = (long long)shopItem->GetPrice().yang;
				line.count = (unsigned int)shopItem->GetInfo().count;
				out.lines.push_back(line);
			}
			return true;
		}
#endif
		return false;
	}

	// A folded query against a stall line: the skill of a book ("ku aura"), or
	// the name with the players' aliases ("fms", "12d", "bodzio").
	bool PlayerBotStallLineMatches(const TPlayerBotStallLine& line, const std::vector<std::string>& candidates,
			bool book, DWORD skillVnum)
	{
		if (book)
			return line.skill != 0 && line.skill == skillVnum;
		return playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(line.name.c_str()), candidates);
	}

	// "ku aura miecza" / "ksiege aura miecza": the skill a book query names, or 0.
	DWORD GetPlayerBotStallBookQuery(const std::string& folded, std::string& rest)
	{
		rest = folded;
		static const char* const kBook[] = { "ku ", "ksiega ", "ksiege ", "ksiegi ", "instr " };
		for (size_t i = 0; i < sizeof(kBook) / sizeof(kBook[0]); ++i)
		{
			const size_t n = strlen(kBook[i]);
			if (folded.compare(0, n, kBook[i]) == 0)
			{
				rest = folded.substr(n);
				return FindPlayerBotSkillByName(rest.c_str());
			}
		}
		return 0;
	}

	enum EPlayerBotTradeVerb
	{
		PLAYERBOT_TRADE_NONE,
		PLAYERBOT_TRADE_BUY,
		PLAYERBOT_TRADE_SELL
	};

	// "Kupię KU Aura", "sprzedam kosc niedzwiedzia", "Szukam Amuletu Orka":
	// the verb, whether a skill book is meant, and the rest folded.
	EPlayerBotTradeVerb ParsePlayerBotTradeText(const char* text, char* outQuery,
			size_t size, bool& outBook)
	{
		outQuery[0] = 0;
		outBook = false;
		char folded[CHAT_MAX_LEN + 1];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		const char* p = folded;
		while (*p && IsPlayerBotChatSeparator(*p))
			++p;
		static const struct { const char* word; EPlayerBotTradeVerb verb; } kVerbs[] = {
			{ "kupie", PLAYERBOT_TRADE_BUY }, { "kupuje", PLAYERBOT_TRADE_BUY },
			{ "szukam", PLAYERBOT_TRADE_BUY }, { "potrzebuje", PLAYERBOT_TRADE_BUY },
			{ "sprzedam", PLAYERBOT_TRADE_SELL }, { "sprzedaje", PLAYERBOT_TRADE_SELL },
			{ "oddam", PLAYERBOT_TRADE_SELL }, { "s>", PLAYERBOT_TRADE_SELL },
			{ "k>", PLAYERBOT_TRADE_BUY },
		};
		EPlayerBotTradeVerb verb = PLAYERBOT_TRADE_NONE;
		for (size_t i = 0; i < sizeof(kVerbs) / sizeof(kVerbs[0]); ++i)
		{
			const size_t len = strlen(kVerbs[i].word);
			if (strncmp(p, kVerbs[i].word, len) == 0 &&
					(p[len] == 0 || IsPlayerBotChatSeparator(p[len])))
			{
				verb = kVerbs[i].verb;
				p += len;
				break;
			}
		}
		if (verb == PLAYERBOT_TRADE_NONE)
			return verb;
		while (*p && IsPlayerBotChatSeparator(*p))
			++p;
		if (strncmp(p, "ku ", 3) == 0)
		{
			outBook = true;
			p += 3;
		}
		else if (strncmp(p, "ksiege ", 7) == 0 || strncmp(p, "ksiega ", 7) == 0 ||
				strncmp(p, "ksiegi ", 7) == 0)
		{
			outBook = true;
			p += 7;
		}
		while (*p && IsPlayerBotChatSeparator(*p))
			++p;
		strlcpy(outQuery, p, size);
		size_t n = strlen(outQuery);
		while (n > 0 && IsPlayerBotChatSeparator(outQuery[n - 1]))
			outQuery[--n] = 0;
		// "Kupie KK", "Sprzedam KD": two letters are too few to search names
		// with, but a word of the players' dictionary names the item exactly.
		return n >= PLAYERBOT_TRADE_QUERY_MIN || (n > 0 && playerbot_conv::IsItemAliasWord(outQuery))
				? verb : PLAYERBOT_TRADE_NONE;
	}

	// "Kupie X": the nearest open counter with X on it answers with where and
	// how much. The player's own map first, then any.
	bool AnswerPlayerBotBuyShout(LPCHARACTER player, const char* query, bool book,
			DWORD skillVnum)
	{
		std::vector<std::string> candidates;
		playerbot_conv::ExpandItemQuery(query ? query : "", candidates);
		LPCHARACTER bestKeeper = NULL;
		TPlayerBotStallLine bestLine;
		long bestMap = 0;
		long long bestDistance = -1;
		TPlayerBotStall stall;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end(); ++it)
		{
			LPCHARACTER keeper = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!keeper || !GetPlayerBotStall(it->first, keeper, stall) || stall.channel != g_bChannel)
				continue;
			for (size_t k = 0; k < stall.lines.size(); ++k)
			{
				const TPlayerBotStallLine& line = stall.lines[k];
				if (!PlayerBotStallLineMatches(line, candidates, book, skillVnum))
					continue;
				const long long distance = stall.mapIndex == player->GetMapIndex()
						? (long long)DISTANCE_APPROX(player->GetX() - stall.x, player->GetY() - stall.y)
						: 1000000LL + (long long)stall.mapIndex;
				if (bestDistance < 0 || distance < bestDistance)
				{
					bestDistance = distance;
					bestKeeper = keeper;
					bestLine = line;
					bestMap = stall.mapIndex;
				}
				break;
			}
		}
		if (!bestKeeper)
			return false;
		char reply[CHAT_MAX_LEN + 1];
		if (bestLine.count > 1)
			snprintf(reply, sizeof(reply), "Mam %s x%u na straganie w %s, %s yang za calosc",
					bestLine.name.c_str(), bestLine.count, GetPlayerBotTownName(bestMap),
					playerbot_conv::FormatYang(bestLine.price).c_str());
		else
			snprintf(reply, sizeof(reply), "Mam %s na straganie w %s, %s yang",
					bestLine.name.c_str(), GetPlayerBotTownName(bestMap),
					playerbot_conv::FormatYang(bestLine.price).c_str());
		SendPlayerBotWhisper(bestKeeper, player, reply);
		return true;
	}

	// The anti-flag that keeps a class off an item, for a weapon offered by
	// name: the proto says who may not carry it.
	DWORD GetPlayerBotJobAntiFlag(BYTE bJob)
	{
		switch (bJob)
		{
			case JOB_WARRIOR: return ITEM_ANTIFLAG_WARRIOR;
			case JOB_ASSASSIN: return ITEM_ANTIFLAG_ASSASSIN;
			case JOB_SURA: return ITEM_ANTIFLAG_SURA;
			case JOB_SHAMAN: return ITEM_ANTIFLAG_SHAMAN;
			default: return 0;
		}
	}

	// "Sprzedam X": a bot that is short of X says it will buy, and where. The
	// bot can: playerbot_market.h reads a player's counter like any other.
	bool AnswerPlayerBotSellShout(LPCHARACTER player, const char* query, bool book,
			DWORD skillVnum)
	{
		DWORD wantedVnum = 0;
		const char* pszName = NULL;
		if (!book)
		{
			// "Sprzedam FMS" is the players' dictionary as much as "Kupie FMS".
			std::vector<std::string> candidates;
			playerbot_conv::ExpandItemQuery(query ? query : "", candidates);
			// A refine material, or a level-30 weapon: the two things a bot
			// reliably wants from anybody.
			const std::set<DWORD>& materials = GetPlayerBotRefineMaterialVnums();
			for (std::set<DWORD>::const_iterator m = materials.begin();
					m != materials.end() && wantedVnum == 0; ++m)
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(*m);
				if (!proto)
					continue;
				if (playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(proto->szLocaleName), candidates))
				{
					wantedVnum = *m;
					pszName = proto->szLocaleName;
				}
			}
			for (DWORD vnum = 290; vnum <= 7169 && wantedVnum == 0; ++vnum)
			{
				if (!IsPlayerBotSpecialLevel30WeaponVnum(vnum))
					continue;
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
				if (!proto)
					continue;
				if (playerbot_conv::ItemNameMatchesAny(playerbot_conv::FoldName(proto->szLocaleName), candidates))
				{
					wantedVnum = vnum;
					pszName = proto->szLocaleName;
				}
			}
			if (wantedVnum == 0)
				return false;
		}

		LPCHARACTER buyer = NULL;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin();
				it != s_mapPlayerBotAIStates.end() && !buyer; ++it)
		{
			LPCHARACTER bot = CHARACTER_MANAGER::instance().FindByPID(it->first);
			if (!bot || !bot->IsItemLoaded() || bot->GetMyShop() || !CanPlayerBotAffordMarket(bot))
				continue;
			if (book)
			{
				if (IsPlayerBotOwnSkill(bot, skillVnum) &&
						bot->GetSkillMasterType(skillVnum) == SKILL_MASTER &&
						bot->GetSkillLevel(skillVnum) >= 20 && bot->GetSkillLevel(skillVnum) < 30)
					buyer = bot;
			}
			else if (IsPlayerBotSpecialLevel30WeaponVnum(wantedVnum))
			{
				const TItemTable* proto = ITEM_MANAGER::instance().GetTable(wantedVnum);
				if (proto && bot->GetLevel() >= 30 && !HasPlayerBotSpecialLevel30Weapon(bot, false) &&
						!IS_SET(proto->dwAntiFlags, GetPlayerBotJobAntiFlag(bot->GetJob())))
					buyer = bot;
			}
			else if (PlayerBotNeedsRefineMaterial(bot, wantedVnum))
				buyer = bot;
		}
		if (!buyer)
			return false;
		char reply[CHAT_MAX_LEN + 1];
		if (book)
			snprintf(reply, sizeof(reply), "Kupie KU %s - wystaw na straganie w Joan albo Bokjung, boty tam kupuja",
					GetPlayerBotSkillName(skillVnum));
		else
			snprintf(reply, sizeof(reply), "Kupie %s - wystaw na straganie w Joan albo Bokjung, boty tam kupuja",
					pszName ? pszName : query);
		SendPlayerBotWhisper(buyer, player, reply);
		return true;
	}

	// ------------------------------------------------------------------
	// "Luruj" / "przestan lurowac": a person's standing order to a bot in
	// their own party.
	//
	// The Archer's luring course (playerbot_lure.h) has been a bot's own role
	// since it was written - it decides for itself when a party is worth
	// pulling for. That decision is not one the AI can make on a person's
	// behalf, so the person asks: one whisper starts the order, another ends
	// it, and while it stands the course is run for that person and the pack
	// is handed to them rather than left on the bot.
	//
	// The order itself is two fields of the bot's state, set here and read
	// there - chat_trade.h is included before lure.h, so nothing in this file
	// may call into the course, and nothing needs to: the course notices the
	// order on its next tick.
	// ------------------------------------------------------------------
	enum EPlayerBotLureOrder
	{
		PLAYERBOT_LURE_ORDER_NONE = playerbot_lure_rules::ORDER_NONE,
		PLAYERBOT_LURE_ORDER_START = playerbot_lure_rules::ORDER_START,
		PLAYERBOT_LURE_ORDER_STOP = playerbot_lure_rules::ORDER_STOP
	};

	// The words themselves are playerbot_lure_order_rules.h, which is pure and
	// unit-tested; this half is the fold from CP1250 that it expects.
	EPlayerBotLureOrder ParsePlayerBotLureOrder(const char* text)
	{
		char folded[CHAT_MAX_LEN + 1];
		FoldPlayerBotChatText(text, folded, sizeof(folded));
		switch (playerbot_lure_rules::ParseOrder(folded))
		{
			case playerbot_lure_rules::ORDER_START: return PLAYERBOT_LURE_ORDER_START;
			case playerbot_lure_rules::ORDER_STOP:  return PLAYERBOT_LURE_ORDER_STOP;
			default:                                return PLAYERBOT_LURE_ORDER_NONE;
		}
	}

	bool HandlePlayerBotLureOrder(LPCHARACTER player, LPCHARACTER bot,
			const char* text, DWORD dwNow)
	{
		const EPlayerBotLureOrder order = ParsePlayerBotLureOrder(text);
		if (order == PLAYERBOT_LURE_ORDER_NONE)
			return false;
		TPlayerBotAIStateMap::iterator it = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
		if (it == s_mapPlayerBotAIStates.end())
			return false;
		TPlayerBotAIState& state = it->second;
		char reply[CHAT_MAX_LEN + 1];

		if (order == PLAYERBOT_LURE_ORDER_STOP)
		{
			if (state.dwLurePlayerPID != player->GetPlayerID())
				snprintf(reply, sizeof(reply), "Nie luruje dla ciebie");
			else
			{
				sys_log(0, "PLAYERBOT_LURE: order ended pid=%u name=%s player=%s held_ms=%u",
						bot->GetPlayerID(), bot->GetName(), player->GetName(),
						state.dwLurePlayerTime != 0 ? dwNow - state.dwLurePlayerTime : 0);
				state.dwLurePlayerPID = 0;
				state.dwLurePlayerTime = 0;
				snprintf(reply, sizeof(reply), "Dobra, koncze lurowanie");
			}
			SendPlayerBotWhisper(bot, player, reply);
			return true;
		}

		// Why a bot cannot take the order, in its own words. Every one of these
		// is something the person can put right in a few seconds, which is why
		// each has a sentence of its own instead of one "nie moge".
		const char* refuse = NULL;
		LPITEM weapon = bot->GetWear(WEAR_WEAPON);
		if (!bot->GetParty() || bot->GetParty() != player->GetParty())
			refuse = "Najpierw zapros mnie do druzyny";
		else if (bot->GetMapIndex() != player->GetMapIndex())
			refuse = "Nie stoje na twojej mapie";
		else if (!IsPlayerBotArcherBuild(bot))
			refuse = "Nie jestem lucznikiem - lurowanie robie z luku";
		else if (!weapon || weapon->GetType() != ITEM_WEAPON ||
				weapon->GetSubType() != WEAPON_BOW)
			refuse = "Nie mam teraz luku w rece";
		// The course refuses a safe zone anyway - there is nothing there to
		// pull - but it refuses it silently, and a person who typed "luruj" in
		// a village and heard "jasne" would be waiting for something that can
		// never happen.
		else if (IsPlayerBotSafeZone(bot->GetMapIndex(), bot->GetX(), bot->GetY()))
			refuse = "Jestesmy w strefie bezpieczenstwa - wyjdz na lowisko i powtorz";
		if (refuse)
		{
			SendPlayerBotWhisper(bot, player, refuse);
			return true;
		}

		if (state.dwLurePlayerPID == player->GetPlayerID())
		{
			// Asking again renews the order rather than restarting it: a person
			// who types it twice does not want the course in progress dropped.
			state.dwLurePlayerTime = dwNow;
			snprintf(reply, sizeof(reply), "Juz dla ciebie luruje");
		}
		else
		{
			state.dwLurePlayerPID = player->GetPlayerID();
			state.dwLurePlayerTime = dwNow;
			// Whatever the role was waiting out is not this person's wait.
			state.dwLureNextTime = 0;
			sys_log(0, "PLAYERBOT_LURE: order taken pid=%u name=%s level=%u player=%s map=%ld",
					bot->GetPlayerID(), bot->GetName(), bot->GetLevel(),
					player->GetName(), bot->GetMapIndex());
			snprintf(reply, sizeof(reply),
					"Jasne. Stoj w miejscu, przyprowadze je na ciebie. Koniec: napisz \"przestan lurowac\"");
		}
		SendPlayerBotWhisper(bot, player, reply);
		return true;
	}

	bool PlayerBotTradeReplyAllowed(LPCHARACTER player, DWORD dwNow)
	{
		DWORD& last = s_mapPlayerBotTradeReplyTime[player->GetPlayerID()];
		if (last != 0 && dwNow - last < PLAYERBOT_TRADE_REPLY_INTERVAL)
			return false;
		last = dwNow;
		return true;
	}

	// A player's shout, after it has gone out on the channel.
	void HandlePlayerShoutForTrade(LPCHARACTER player, const char* text)
	{
		if (!player || !text)
			return;
		char query[128];
		bool book = false;
		const EPlayerBotTradeVerb verb = ParsePlayerBotTradeText(text, query, sizeof(query), book);
		if (verb == PLAYERBOT_TRADE_NONE)
			return;
		const DWORD skillVnum = book ? FindPlayerBotSkillByName(query) : 0;
		if (book && skillVnum == 0)
		{
			// "Kupie ksiege misji" names an item whose name begins with the
			// word, not a skill: it is searched as a name like any other.
			char named[sizeof(query)];
			snprintf(named, sizeof(named), "ksiega %s", query);
			strlcpy(query, named, sizeof(query));
			book = false;
		}
		const DWORD dwNow = get_dword_time();
		if (!PlayerBotTradeReplyAllowed(player, dwNow))
			return;
		const bool answered = verb == PLAYERBOT_TRADE_BUY
				? AnswerPlayerBotBuyShout(player, query, book, skillVnum)
				: AnswerPlayerBotSellShout(player, query, book, skillVnum);
		sys_log(0, "PLAYERBOT_TRADE: shout from=%s verb=%s book=%d query=\"%s\" answered=%d",
				player->GetName(), verb == PLAYERBOT_TRADE_BUY ? "buy" : "sell",
				book ? 1 : 0, query, answered ? 1 : 0);
	}

	// A player's whisper to a bot. A trade line is answered like a shout, by
	// whichever bot is best placed; anything else gets the bot's own state -
	// what its counter holds, or that it is out hunting.
	void HandlePlayerWhisperToBot(LPCHARACTER player, LPCHARACTER bot, const char* text)
	{
		if (!player || !bot || !text)
			return;
		const DWORD dwNow = get_dword_time();
		// Before the trade line, because an order is answered whatever the
		// reply clock says: a person who asked a bot to pull for them is owed
		// an answer, and "luruj" is nobody's idea of a trade.
		if (HandlePlayerBotLureOrder(player, bot, text, dwNow))
			return;
		char query[128];
		bool book = false;
		const EPlayerBotTradeVerb verb = ParsePlayerBotTradeText(text, query, sizeof(query), book);
		if (verb != PLAYERBOT_TRADE_NONE)
		{
			// The 8-second trade clock is for shouts - one bot to one door. A
			// whisper inside it used to vanish without a word; now this bot
			// answers it itself from its own counter and needs (conversation
			// layer, I_BUY / I_SELL), so nothing a person writes is lost.
			std::map<DWORD, DWORD>::const_iterator last =
					s_mapPlayerBotTradeReplyTime.find(player->GetPlayerID());
			if (last != s_mapPlayerBotTradeReplyTime.end() && last->second != 0 &&
					dwNow - last->second < PLAYERBOT_TRADE_REPLY_INTERVAL &&
					HandlePlayerBotConversation(player, bot, text))
				return;
			HandlePlayerShoutForTrade(player, text);
			return;
		}
		// Ordinary conversation: analysed at once, answered from the
		// conversation queue 0.7-1.5 s later, merged when several lines come
		// together. No limiter drops anything (playerbot_conv_engine.h).
		char reply[CHAT_MAX_LEN + 1];
		TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.find(bot->GetPlayerID());
		if (HandlePlayerBotConversation(player, bot, text))
			return;
		TPlayerBotStall stall;
		if (GetPlayerBotStall(bot->GetPlayerID(), bot, stall) && !stall.lines.empty())
		{
			std::string goods;
			for (size_t k = 0; k < stall.lines.size() && k < 3; ++k)
			{
				if (!goods.empty())
					goods += ", ";
				goods += stall.lines[k].name;
			}
			snprintf(reply, sizeof(reply), "Mam stragan w %s, na nim: %s",
					GetPlayerBotTownName(stall.mapIndex), goods.c_str());
			SendPlayerBotWhisper(bot, player, reply);
		}
		else if (it != s_mapPlayerBotAIStates.end() && it->second.bMarketTrip)
		{
			snprintf(reply, sizeof(reply), "Wlasnie ide na targ w %s", GetPlayerBotTownName(bot->GetMapIndex()));
			SendPlayerBotWhisper(bot, player, reply);
		}
		else
		{
			snprintf(reply, sizeof(reply), "Nie rozumiem. Zapytaj mnie, co robie, gdzie expie albo co mam na straganie.");
			SendPlayerBotWhisper(bot, player, reply);
		}
	}
}

#endif
