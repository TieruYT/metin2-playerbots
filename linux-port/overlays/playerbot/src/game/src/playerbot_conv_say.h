#ifndef __INC_METIN2_PLAYERBOT_CONV_SAY_H__
#define __INC_METIN2_PLAYERBOT_CONV_SAY_H__

// PlayerBot Conversation v6 - how a line is said (pure).
//
// RESPONSE CANDIDATE SELECTION
//
// Every answer is chosen from a set of variants. Pick() never repeats a
// variant the pair heard recently (TConvMemory::recentTemplates), and Fill()
// puts the bot's real state into the $PLACEHOLDERS. Nothing here decides WHAT
// to say - the generators do - only which of the equivalent ways to say it.

#include "playerbot_conv_state.h"

namespace playerbot_conv
{
	struct TGen
	{
		const TBotSnapshot& s;
		TConvMemory& m;
		TRng& rng;
		IConvWorld* world;
		const TAnalysis* a;   // the line being answered
		int tier;
		int voice;
		u32 now;
		// Group state: several questions answered in one whisper.
		size_t index;
		size_t groupSize;
		bool saidMap;
		bool saidActivity;
		bool saidParty;
		bool saidGreeting;
		bool groupHasActivity; // an I_ACTIVITY is answered in this same whisper
		std::string askBack;  // at most one question back per whisper
		unsigned char askBackKind;
		ETopic askBackTopic;
		std::string reason;   // why - for a later "dlaczego?"

		TGen(const TBotSnapshot& snap, TConvMemory& mem, TRng& r, IConvWorld* w, u32 t)
			: s(snap), m(mem), rng(r), world(w), a(NULL), tier(TIER_STRANGER),
			voice(VoiceOf(snap.style)), now(t), index(0), groupSize(1), saidMap(false),
			saidActivity(false), saidParty(false), saidGreeting(false), groupHasActivity(false), askBackKind(ASK_NONE),
			askBackTopic(T_NONE) {}

		bool Merged() const { return groupSize > 1; }
		bool Bad() const { return s.mood == MOOD_BAD; }
		bool Good() const { return s.mood == MOOD_GOOD; }
		bool LowHp() const { return !s.dead && s.hpPct > 0 && s.hpPct < 30; }
	};

	inline const char* Pick(TGen& g, const char* const* variants, size_t n)
	{
		if (!variants || n == 0)
			return "";
		const size_t start = g.rng.Range((u32)n);
		for (size_t k = 0; k < n; ++k)
		{
			const char* v = variants[(start + k) % n];
			const u32 id = HashStr(v, 0x5eed);
			if (!g.m.UsedTemplateRecently(id))
			{
				g.m.NoteTemplate(id);
				return v;
			}
		}
		const char* v = variants[start];
		g.m.NoteTemplate(HashStr(v, 0x5eed));
		return v;
	}

#define PBC_PICK(g, arr) Pick((g), (arr), sizeof(arr) / sizeof((arr)[0]))

	inline std::string Fill(const TGen& g, const char* tpl)
	{
		std::string out = tpl ? tpl : "";
		if (out.find('$') == std::string::npos)
			return out;
		const TBotSnapshot& s = g.s;
		const TMapWords& map = GetMapWords(s.mapIndex);
		const TMapWords& dest = GetMapWords(s.travelMap);
		// Longest names first, so $MAPIN is not eaten by $MAP.
		ReplaceAll(out, "$MAPINSHORT", map.atShort);
		ReplaceAll(out, "$MAPIN", map.at);
		ReplaceAll(out, "$MAPNAME", *map.name ? map.name : "ta mapa");
		ReplaceAll(out, "$DEST", *dest.to ? dest.to : "dalej");
		ReplaceAll(out, "$TARGET", s.targetName.empty() ? std::string("moby") : s.targetName);
		{
			const char* fam = MobFamilyPlural(FoldName(s.targetName.c_str()));
			ReplaceAll(out, "$FAMILY", *fam ? fam : "moby");
		}
		ReplaceAll(out, "$NEXTLVL", ToString(s.level + 1));
		ReplaceAll(out, "$LVL", ToString(s.level));
		ReplaceAll(out, "$PLAYER", s.askerName);
		ReplaceAll(out, "$NAME", s.name);
		ReplaceAll(out, "$GOLD", FormatYang(s.gold));
		ReplaceAll(out, "$PARTYN", ToString(s.partySize));
		ReplaceAll(out, "$LEADER", s.partyLeader.empty() ? std::string("ktos") : s.partyLeader);
		ReplaceAll(out, "$GUILDN", ToString(s.guildMembers));
		ReplaceAll(out, "$GUILD", s.guildName);
		ReplaceAll(out, "$HORSELVL", ToString(s.horseLevel));
		ReplaceAll(out, "$FREE", ToString(s.freeCells));
		ReplaceAll(out, "$HP", ToString(s.hpPct));
		ReplaceAll(out, "$SP", ToString(s.spPct));
		ReplaceAll(out, "$CLASS", ClassName(s.job));
		ReplaceAll(out, "$EMPIRE", EmpireName(s.empire));
		ReplaceAll(out, "$WEAPON", s.weaponName.empty() ? std::string("nic") : s.weaponName);
		ReplaceAll(out, "$WPLUS", ToString(s.weaponPlus));
		ReplaceAll(out, "$ARMOR", s.armorName.empty() ? std::string("nic") : s.armorName);
		ReplaceAll(out, "$APLUS", ToString(s.armorPlus));
		ReplaceAll(out, "$BIO", s.bioWanted);
		ReplaceAll(out, "$HUNT", s.huntMob);
		ReplaceAll(out, "$HUNTN", ToString(s.huntRemaining));
		ReplaceAll(out, "$MOBS", ToString(s.mobsNear < 0 ? 0 : s.mobsNear));
		{
			const TMapWords& shopMap = GetMapWords(s.shopMapIndex);
			ReplaceAll(out, "$SHOPAT", *shopMap.name ? std::string(shopMap.at) : std::string("w miescie"));
		}
		ReplaceAll(out, "$SM", ToString(s.dragonCoins));
		ReplaceAll(out, "$SHOPN", ToString(s.shopItems));
		ReplaceAll(out, "$SHOP", s.shopSummary);
		ReplaceAll(out, "$ONLINE", ToString((long long)s.onlineMinutes));
		if (g.a)
		{
			ReplaceAll(out, "$OBJB", g.a->objectB);
			ReplaceAll(out, "$OBJ", g.a->object);
		}
		ReplaceAll(out, "$LIKES", g.m.playerLikes);
		return out;
	}

	inline std::string Say(TGen& g, const char* const* variants, size_t n)
	{
		return Fill(g, Pick(g, variants, n));
	}

#define PBC_SAY(g, arr) Say((g), (arr), sizeof(arr) / sizeof((arr)[0]))

	// Join two sentences with one space, each ending in punctuation.
	inline void Append(std::string& out, const std::string& piece)
	{
		if (piece.empty())
			return;
		if (!out.empty())
		{
			const char last = out[out.size() - 1];
			const bool smiley = out.size() >= 2 && (out[out.size() - 2] == ':' || out[out.size() - 2] == 'x') &&
					(last == 'D' || last == 'P');
			if (last != '.' && last != '!' && last != '?' && last != ')' && last != ':' && !smiley)
				out += '.';
			out += ' ';
		}
		out += piece;
	}

	// "$OBJ? ..." only when the object is worth echoing.
	inline bool HasEchoObject(const TGen& g)
	{
		return g.a && !g.a->object.empty() && g.a->object.size() <= 24;
	}

	inline std::string EchoObject(const TGen& g)
	{
		std::string o = g.a ? g.a->object : std::string();
		CapitalizeFirst(o);
		return o;
	}

	// Deterministic per bot and word: the same bot asked twice about winter
	// gives the same opinion, two bots may differ. 0..99.
	inline int OpinionRoll(const TGen& g, const std::string& what, int salt)
	{
		std::string key = what.size() > 4 ? what.substr(0, 4) : what;
		return (int)(HashStr(key.c_str(), g.m.botPID * 2654435761u + (u32)salt) % 100u);
	}
}

#endif
