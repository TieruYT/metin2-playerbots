#ifndef __INC_METIN2_PLAYERBOT_CONV_GENERATOR_H__
#define __INC_METIN2_PLAYERBOT_CONV_GENERATOR_H__

// PlayerBot Conversation v6 - RESPONSE GENERATOR (pure).
//
// Intent + Context + AI State + Persona + Mood + Relationship + Memory
//   -> candidate set -> one natural line.
//
// Rules the generators keep:
//   - every fact comes from TBotSnapshot (no guild -> never "nasza gildia",
//     alone -> never "moi koledzy z PT", 20% HP -> never "swietna forma"),
//   - answers are spoken, not reported: "Wlasnie bije orki w Dolinie", not
//     "Moim aktualnym dzialaniem jest walka",
//   - the voice (persona) colours a line, it does not limit the topic,
//   - the mood shows sometimes, not in every line.
//
// One function per intent; GenerateOne() dispatches. The merger and the queue
// are playerbot_conv_engine.h.

#include "playerbot_conv_general.h"

namespace playerbot_conv
{
	// ------------------------------------------------------------- helpers

	inline bool Fighting(const TGen& g) { return g.s.action == A_FIGHT || g.s.action == A_LURE; }

	inline bool HasFamily(const TGen& g)
	{
		return *MobFamilyPlural(FoldName(g.s.targetName.c_str())) != 0;
	}

	inline const char* GoalPhrase(const TGen& g)
	{
		switch (g.s.goal)
		{
			case G_SURVIVE: return "odzyskac sily";
			case G_PROFESSION: return "wybrac profesje";
			case G_EQUIPMENT: return "zdobyc lepszy sprzet";
			case G_RESTOCK: return "uzupelnic zapasy";
			case G_REFINE: return "ulepszyc bron";
			case G_SKILL: return "podbic umiejetnosci";
			case G_METIN: return "polowac na Metiny";
			case G_PARTY_CHALLENGE: return "znalezc cos mocniejszego dla ekipy";
			case G_BIOLOGIST: return "zrobic zadania dla Biologa";
			case G_HUNTING: return "dokonczyc polowanie";
			case G_HORSE: return "zajac sie koniem";
			case G_FISHING: return "troche polowic";
			default: return "wbijac kolejne poziomy";
		}
	}

	inline std::string BagClause(TGen& g)
	{
		if (g.s.bagCells > 0 && g.s.freeCells <= 2 && !g.s.inTown)
		{
			static const char* const k[] = {
				"Zaraz musze wracac do miasta, EQ mam pelne.",
				"Tylko EQ mi sie konczy, zaraz trzeba bedzie sprzedac drop." };
			return PBC_SAY(g, k);
		}
		return std::string();
	}

	inline std::string VoiceFlavour(TGen& g)
	{
		if (g.Merged() || g.Bad() || !g.rng.Chance(25))
			return std::string();
		switch (g.voice)
		{
			case V_GRINDER:
			{
				static const char* const k[] = { "Byle do nastepnego poziomu.", "Exp sam sie nie zrobi." };
				return PBC_SAY(g, k);
			}
			case V_MERCHANT:
			{
				static const char* const k[] = { "Moze cos cennego wypadnie na sprzedaz.", "Drop pojdzie potem na targ." };
				return PBC_SAY(g, k);
			}
			case V_FIGHTER:
				if (!g.s.targetStone)
				{
					static const char* const k[] = { "Szkoda, ze nie ma tu Metinow.", "Moglyby byc mocniejsze." };
					return PBC_SAY(g, k);
				}
				return std::string();
			case V_SOCIAL:
				if (!g.s.inParty)
				{
					static const char* const k[] = { "Samemu troche nudno.", "Przydalby sie ktos do towarzystwa." };
					return PBC_SAY(g, k);
				}
				return std::string();
			default:
			{
				static const char* const k[] = { "Ladna okolica swoja droga.", "Spokojnie tu." };
				return PBC_SAY(g, k);
			}
		}
	}

	// --------------------------------------------------------------- state

	inline std::string ActivityClause(TGen& g, bool withMap)
	{
		const TBotSnapshot& s = g.s;
		const bool map = withMap && IsKnownMap(s.mapIndex);
		if (s.dead)
		{
			static const char* const k[] = { "Wlasnie zginalem, czekam az wstane.", "Leze na ziemi, ktos mnie ubil." };
			return PBC_SAY(g, k);
		}
		if (s.afk)
		{
			static const char* const k[] = { "Mam chwile przerwy.", "Robie sobie krotka przerwe." };
			return PBC_SAY(g, k);
		}
		switch (s.action)
		{
			case A_FIGHT:
			{
				if (g.LowHp())
				{
					static const char* const k[] = {
						"Walcze, ale mam juz malo HP. Zaraz bede musial odpoczac.",
						"Bije sie, ale ledwo stoje. Zaraz sie wycofam." };
					return PBC_SAY(g, k);
				}
				if (s.targetStone)
				{
					static const char* const kMap[] = { "Rozbijam Metina $MAPIN.", "Bije Metina $MAPINSHORT, zaraz padnie." };
					static const char* const kNo[] = { "Rozbijam Metina.", "Bije Metina, zaraz padnie." };
					return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
				}
				if (s.targetBoss)
				{
					static const char* const k[] = { "Bije bossa, trzymaj kciuki!", "Walcze z bossem, zaraz pogadamy." };
					return PBC_SAY(g, k);
				}
				if (s.targetPlayer)
				{
					static const char* const k[] = { "Bije sie z kims, chwila.", "Mam pojedynek, zaraz." };
					return PBC_SAY(g, k);
				}
				if (HasFamily(g))
				{
					static const char* const kMap[] = { "Bije $FAMILY $MAPIN.", "Expie $MAPIN, leca $FAMILY.", "Wlasnie ubijam $FAMILY $MAPINSHORT." };
					static const char* const kNo[] = { "Bije $FAMILY.", "Wlasnie ubijam $FAMILY.", "Leca $FAMILY, expie." };
					return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
				}
				static const char* const kMap[] = { "Wlasnie expie $MAPIN.", "Bije moby $MAPIN.", "Expie sobie $MAPINSHORT." };
				static const char* const kNo[] = { "Wlasnie bije moby.", "Expie.", "Bije, co popadnie." };
				return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
			}
			case A_TRAVEL:
				if (IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
				{
					if (s.riding)
					{
						static const char* const k[] = { "Jade na koniu $DEST.", "Jestem w drodze $DEST, na koniu." };
						return PBC_SAY(g, k);
					}
					static const char* const k[] = { "Jestem w drodze $DEST.", "Ide $DEST.", "Zmierzam $DEST." };
					return PBC_SAY(g, k);
				}
				else
				{
					static const char* const kMap[] = { "Ide na inny spot $MAPIN.", "Przemieszczam sie $MAPINSHORT." };
					static const char* const kNo[] = { "Ide na inny spot.", "Jestem w drodze." };
					return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
				}
			case A_LOOT:
			{
				static const char* const k[] = { "Zbieram drop po walce.", "Podnosze, co wypadlo." };
				return PBC_SAY(g, k);
			}
			case A_RECOVER:
			{
				static const char* const k[] = { "Odpoczywam chwile, zbieram HP.", "Siadlem na chwile, regeneruje sie." };
				return PBC_SAY(g, k);
			}
			case A_TOWN_REST:
			{
				static const char* const k[] = { "Siedze w miescie i odpoczywam.", "Odpoczywam $MAPIN." };
				return PBC_SAY(g, k);
			}
			case A_TRAIN:
			{
				static const char* const k[] = { "Zalatwiam sprawy u trenera.", "Ogarniam profesje." };
				return PBC_SAY(g, k);
			}
			case A_SHOP:
			{
				static const char* const k[] = { "Robie zakupy u handlarza.", "Kupuje potki i takie tam." };
				return PBC_SAY(g, k);
			}
			case A_REFINE:
			{
				static const char* const k[] = { "Ulepszam sprzet u kowala. Trzymaj kciuki.", "Stoje u kowala, ulepszam bron." };
				return PBC_SAY(g, k);
			}
			case A_READ_BOOK:
			{
				static const char* const k[] = { "Czytam ksiegi umiejetnosci.", "Ucze sie z ksiag." };
				return PBC_SAY(g, k);
			}
			case A_SOCKET:
			{
				static const char* const k[] = { "Wkladam kamienie duszy do broni.", "Bawie sie kamieniami duszy." };
				return PBC_SAY(g, k);
			}
			case A_PARTY_ASSEMBLE:
			{
				static const char* const k[] = { "Zbieram ekipe na cos wiekszego.", "Czekam, az sie ekipa zbierze." };
				return PBC_SAY(g, k);
			}
			case A_BIOLOGIST:
				if (!s.bioWanted.empty())
				{
					static const char* const k[] = { "Zbieram $BIO dla Biologa.", "Robie zadanie Biologa, szukam $BIO." };
					return PBC_SAY(g, k);
				}
				else
				{
					static const char* const k[] = { "Robie zadanie dla Biologa.", "Zalatwiam sprawy u Biologa." };
					return PBC_SAY(g, k);
				}
			case A_STABLE:
			{
				static const char* const k[] = { "Jestem u Stajennego, ogarniam konia.", "Zajmuje sie koniem." };
				return PBC_SAY(g, k);
			}
			case A_STALL:
			{
				static const char* const kMap[] = { "Stoje ze straganem $MAPIN.", "Handluje, mam stragan $MAPIN." };
				static const char* const kNo[] = { "Stoje ze straganem.", "Handluje przy straganie." };
				return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
			}
			case A_FISHING:
			{
				static const char* const kMap[] = { "Lowie ryby $MAPIN.", "Siedze z wedka $MAPINSHORT." };
				static const char* const kNo[] = { "Lowie ryby.", "Siedze z wedka." };
				return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
			}
			case A_MARKET:
			{
				static const char* const k[] = { "Chodze po targu i szukam okazji.", "Robie zakupy na targu." };
				return PBC_SAY(g, k);
			}
			case A_LURE:
				if (s.luringForAsker)
				{
					static const char* const k[] = { "Przyciagam dla ciebie moby, stoj w miejscu.", "Zbieram ci paczke mobow." };
					return PBC_SAY(g, k);
				}
				else
				{
					static const char* const k[] = { "Podciagam moby dla ekipy.", "Luruje moby dla grupy." };
					return PBC_SAY(g, k);
				}
			case A_MINING:
			{
				static const char* const kMap[] = { "Kopie rude $MAPIN.", "Siedze w kopalni, kopie." };
				static const char* const kNo[] = { "Kopie rude.", "Kopie, ile sie da." };
				return map ? PBC_SAY(g, kMap) : PBC_SAY(g, kNo);
			}
			default:
				if (s.inTown)
				{
					static const char* const k[] = { "Krece sie po miescie.", "Nic specjalnego, stoje $MAPIN." };
					return PBC_SAY(g, k);
				}
				else
				{
					static const char* const k[] = { "Rozgladam sie, co tu porobic.", "Nic konkretnego, zastanawiam sie, co dalej." };
					return PBC_SAY(g, k);
				}
		}
	}

	inline std::string GenActivity(TGen& g)
	{
		const bool yesNoExp = g.a && g.a->concepts.Has(C_EXP) && !g.a->concepts.Has(C_WHAT);
		std::string out;
		if (yesNoExp)
		{
			if (Fighting(g))
				out = g.rng.Chance(50) ? "Tak. " : "No, expie. ";
			else
				out = "Teraz nie. ";
		}
		out += ActivityClause(g, !g.saidMap);
		if (!g.saidMap && IsKnownMap(g.s.mapIndex) && out.find(GetMapWords(g.s.mapIndex).atShort) != std::string::npos)
			g.saidMap = true;
		g.saidActivity = true;
		Append(out, BagClause(g));
		Append(out, VoiceFlavour(g));
		// A reason ready for "dlaczego?".
		if (Fighting(g))
		{
			static const char* const kWhy[V_COUNT] = {
				"Bo tu jest dobry exp na moj poziom.", "Bo akurat tu trafilem i jest spokojnie.",
				"Bo z tych mobow leci cos, co sie dobrze sprzedaje.", "Bo lubie walke, a tu jest z kim.",
				"Bo tu zawsze ktos jest." };
			g.reason = kWhy[g.voice];
		}
		else if (g.s.action == A_RECOVER || g.s.action == A_TOWN_REST)
			g.reason = "Bo mialem juz malo HP.";
		else if (g.s.action == A_TRAVEL)
			g.reason = "Bo tam mam lepszy spot na moj poziom.";
		else if (g.s.action == A_SHOP || g.s.action == A_MARKET)
			g.reason = "Bo potrzebuje zapasow.";
		// A question back, now and then.
		if (!g.Merged() && !g.Bad() && g.askBack.empty() && g.tier >= TIER_KNOWN && g.rng.Chance(g.voice == V_SOCIAL ? 45 : 20))
		{
			static const char* const k[] = { "A ty co robisz?", "A ty co porabiasz?" };
			g.askBack = PBC_SAY(g, k);
			g.askBackKind = ASK_ACTIVITY;
		}
		return out;
	}

	// "jestes w v1?", "expisz na sohan?", "idziesz do m1?" - the place a player
	// named, resolved to this bot's kingdom. 0 when none was named.
	inline long MentionedMap(const TGen& g)
	{
		if (!g.a || g.a->mentionMap == 0)
			return 0;
		const long m = ResolveMapAlias(g.a->mentionMap, g.s.empire);
		return m == 0 ? MAP_ALIAS_UNLISTED : m;
	}

	inline std::string MapYesNo(TGen& g, long mentioned)
	{
		const TBotSnapshot& s = g.s;
		g.saidMap = true;
		if (mentioned == s.mapIndex && IsKnownMap(s.mapIndex))
		{
			static const char* const k[] = { "Tak, jestem $MAPIN.", "No, $MAPIN." };
			std::string out = PBC_SAY(g, k);
			CapitalizeFirst(out);
			return out;
		}
		if (!IsKnownMap(s.mapIndex))
			return "Nie, jestem gdzie indziej.";
		static const char* const k[] = { "Nie, jestem $MAPIN.", "Nie, teraz $MAPIN." };
		return PBC_SAY(g, k);
	}

	inline std::string GenLocation(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (const long mentioned = MentionedMap(g))
			return MapYesNo(g, mentioned);
		if (!IsKnownMap(s.mapIndex))
		{
			static const char* const k[] = { "Gdzies na uboczu, nawet nie wiem, jak to miejsce sie nazywa.", "W jakims dziwnym miejscu, nie znam nazwy." };
			return PBC_SAY(g, k);
		}
		g.saidMap = true;
		if (s.askerNear)
		{
			static const char* const k[] = { "Tuz obok ciebie :)", "Przeciez stoje niedaleko ciebie, $MAPIN." };
			return PBC_SAY(g, k);
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
		{
			static const char* const k[] = { "Jestem $MAPIN, ale ide $DEST.", "Teraz $MAPIN, zmierzam $DEST." };
			return PBC_SAY(g, k);
		}
		if (g.saidActivity)
		{
			static const char* const k[] = { "Jestem $MAPIN.", "A jestem $MAPIN." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Jestem teraz $MAPIN.", "Siedze $MAPIN.", "Aktualnie $MAPIN.", "Jestem $MAPINSHORT." };
		return PBC_SAY(g, k);
	}

	inline std::string GenActivityLocation(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (const long mentioned = MentionedMap(g))
			return MapYesNo(g, mentioned);
		if (!IsKnownMap(s.mapIndex))
			return GenLocation(g);
		g.saidMap = true;
		// The last whisper already named the map: say so, do not recite it.
		if (!g.Merged() && g.m.lastReply.find(GetMapWords(s.mapIndex).atShort) != std::string::npos &&
				g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
		{
			static const char* const k[] = { "No mowie, $MAPIN :)", "Tutaj, $MAPIN.", "$MAPIN, tak jak pisalem." };
			return PBC_SAY(g, k);
		}
		if (Fighting(g))
		{
			if (g.saidActivity)
			{
				static const char* const k[] = { "$MAPIN.", "Tutaj, $MAPIN." };
				return PBC_SAY(g, k);
			}
			if (HasFamily(g))
			{
				static const char* const k[] = { "$MAPIN, bije $FAMILY.", "Expie $MAPIN.", "$MAPIN, tu jest niezly spot na $FAMILY." };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Expie $MAPIN.", "$MAPIN, tu jest dobry spot.", "Teraz $MAPIN." };
			return PBC_SAY(g, k);
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap))
		{
			static const char* const k[] = { "Teraz nigdzie, dopiero ide $DEST.", "Ide wlasnie $DEST, tam bede expil." };
			return PBC_SAY(g, k);
		}
		if (s.inTown)
		{
			static const char* const k[] = { "Teraz nigdzie, jestem $MAPIN. Potem pewnie wroce na exp.", "Na razie nie expie, stoje $MAPIN." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Teraz nie expie, ale jestem $MAPIN.", "Na razie przerwa, jestem $MAPIN." };
		return PBC_SAY(g, k);
	}

	inline std::string GenMobCount(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.inTown && !Fighting(g))
		{
			static const char* const k[] = { "W miescie? Tu nie ma mobow :)", "Tu sa sami gracze i handlarze, zadnych mobow." };
			return PBC_SAY(g, k);
		}
		const int n = s.mobsNear;
		const bool askedFew = g.a && g.a->polarityNegative;
		std::string out;
		if (n < 0)
		{
			static const char* const k[] = { "Troche jest.", "Da sie expic." };
			out = PBC_SAY(g, k);
		}
		else if (n == 0)
		{
			static const char* const k[] = { "Pusto teraz, wszystko wybite.", "Nic nie ma, czekam na respawn." };
			out = PBC_SAY(g, k);
		}
		else if (n <= 3)
		{
			static const char* const k[] = { "Malo. Ktos tu chyba przede mna czyscil.", "Kilka sztuk, nic wielkiego." };
			out = PBC_SAY(g, k);
		}
		else if (n <= 9)
		{
			static const char* const k[] = { "Troche jest, ale spot jest spokojny.", "Troche ich jest, w sam raz.", "Jest co bic, ale bez szalu." };
			out = PBC_SAY(g, k);
		}
		else if (n <= 19)
		{
			static const char* const kFew[] = { "Wcale nie malo, sporo ich.", "Nie, jest ich calkiem sporo." };
			static const char* const k[] = { "Sporo, jest co bic.", "Sporo ich, exp leci." };
			out = askedFew ? PBC_SAY(g, kFew) : PBC_SAY(g, k);
		}
		else
		{
			static const char* const k[] = { "Duzo! Ledwo nadazam.", "Mnostwo, az sie roi." };
			out = PBC_SAY(g, k);
		}
		if (s.playersNear >= 3 && g.rng.Chance(60))
			Append(out, "Tylko tloczno troche, sporo ludzi.");
		g.reason = n > 9 ? "Bo szybko sie respia." : "Bo ktos tu pewnie wczesniej byl.";
		return out;
	}

	inline std::string GenTarget(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (!Fighting(g) || s.targetName.empty())
		{
			if (Fighting(g))
			{
				static const char* const k[] = { "Nic konkretnego, szukam czegos do bicia.", "Akurat nic, rozgladam sie." };
				return PBC_SAY(g, k);
			}
			static const char* const k[] = { "Teraz nic nie bije.", "Nic, mam przerwe od walki." };
			return PBC_SAY(g, k);
		}
		if (s.targetStone)
		{
			static const char* const k[] = { "Metina! Zaraz go rozwale.", "Kamien Metina, zaraz padnie." };
			return PBC_SAY(g, k);
		}
		if (HasFamily(g))
		{
			static const char* const k[] = { "Bije $FAMILY. Teraz akurat $TARGET.", "$FAMILY, glownie.", "Teraz $TARGET." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Teraz na celowniku: $TARGET.", "Aktualnie $TARGET.", "Bije sie z tym tu, $TARGET." };
		return PBC_SAY(g, k);
	}

	inline std::string GenLevel(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out;
		if (s.expPct >= 85)
		{
			static const char* const k[] = { "Mam $LVL, zaraz wbijam $NEXTLVL.", "$LVL, ale juz prawie $NEXTLVL." };
			out = PBC_SAY(g, k);
		}
		else if (s.expPct >= 60 && g.voice == V_GRINDER)
			out = Fill(g, "Mam $LVL. Jeszcze troche i powinienem wbic kolejny poziom.");
		else
		{
			static const char* const k[] = { "Mam $LVL poziom.", "$LVL lvl.", "Mam $LVL. Powoli do przodu.", "$LVL, na razie." };
			out = PBC_SAY(g, k);
		}
		if (s.askerLevel > 0 && g.rng.Chance(35))
		{
			if (s.askerLevel >= s.level + 10)
				Append(out, "Ty mnie juz troche przegoniles.");
			else if (s.level >= s.askerLevel + 10)
				Append(out, "Troche wyzej od ciebie.");
			else if (s.askerLevel >= s.level - 3 && s.askerLevel <= s.level + 3)
				Append(out, "Mamy podobnie.");
		}
		g.reason = "Bo tyle wyexpilem.";
		return out;
	}

	// The class and its path in the instrumental a sentence needs, with the
	// players' word and the game's in brackets where they differ.
	inline const char* BuildPhrase(int build)
	{
		switch (build)
		{
			case B_BODY: return "wojownikiem body";
			case B_MENTAL: return "wojownikiem mental";
			case B_DAGGER: return "ninja na sztyletach (dagger)";
			case B_ARCHER: return "ninja lucznikiem (archer)";
			case B_WEAPON: return "sura WP (magiczna bron)";
			case B_BLACK_MAGIC: return "sura BM (czarna magia)";
			case B_DRAGON: return "szamanem smok";
			case B_HEAL: return "szamanem heal (leczenie)";
			default: return "";
		}
	}

	// The path's skills, best first - the build's own first skill ahead of an
	// equal one - as "Aura Miecza M3, Wir Miecza 17". Empty before any is
	// learnt.
	inline std::string SkillsList(const TBotSnapshot& s, size_t maxItems)
	{
		int order[6];
		int n = 0;
		for (int i = 0; i < 6; ++i)
		{
			if (s.skillVnums[i] == 0 || s.skillLevels[i] <= 0 || !*SkillNameOf(s.skillVnums[i]))
				continue;
			int at = n++;
			while (at > 0)
			{
				const int prev = order[at - 1];
				const bool before = s.skillLevels[i] > s.skillLevels[prev] ||
						(s.skillLevels[i] == s.skillLevels[prev] && s.skillVnums[i] == s.mainSkill);
				if (!before)
					break;
				order[at] = prev;
				--at;
			}
			order[at] = i;
		}
		std::string out;
		for (int k = 0; k < n && (size_t)k < maxItems; ++k)
		{
			if (!out.empty())
				out += ", ";
			out += SkillNameOf(s.skillVnums[order[k]]);
			out += ' ';
			out += SkillGradeText(s.skillLevels[order[k]]);
		}
		return out;
	}

	inline std::string GenClass(TGen& g)
	{
		static const char* const k[] = { "Gram $CLASSI.", "Jestem $CLASSI.", "$CLASSI, od poczatku." };
		std::string out = Pick(g, k, 3);
		const int build = g.s.Build();
		ReplaceAll(out, "$CLASSI", build != B_NONE ? BuildPhrase(build) : ClassNameInstr(g.s.job));
		CapitalizeFirst(out);
		return out;
	}

	// "jaka masz profesje?", "jestes body czy mental?", "grasz archerem?" - the
	// path, a yes or a no when the line named one, the level and the best
	// skill of the path.
	inline std::string GenBuild(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		const int build = s.Build();
		if (build == B_NONE)
		{
			g.reason = "Bo sciezke wybiera sie u trenera od piatego poziomu.";
			if (s.level < 5)
				return Fill(g, "Jeszcze nie mam sciezki, mam dopiero $LVL poziom. Wybiera sie ja od piatego.");
			return "Jeszcze nie wybralem sciezki, musze isc do trenera.";
		}
		const unsigned int named = g.a ? NamedBuildsInLine(g.a->tokens, g.a->concepts) : 0;
		const unsigned int mine = 1u << build;
		std::string out;
		if (named != 0 && (named & mine) == 0)
			out = std::string("Nie, gram ") + BuildPhrase(build) + ".";
		else if (named == mine)
		{
			static const char* const k[] = { "Tak, gram $P.", "Zgadza sie, jestem $P.", "Tak, jestem $P." };
			out = Pick(g, k, 3);
			ReplaceAll(out, "$P", BuildPhrase(build));
		}
		else
		{
			static const char* const k[] = { "Gram $P.", "Jestem $P.", "Gram $P, tak wybralem u trenera." };
			out = Pick(g, k, 3);
			ReplaceAll(out, "$P", BuildPhrase(build));
		}
		CapitalizeFirst(out);
		std::string tail = Fill(g, "Mam $LVL poziom");
		const std::string best = SkillsList(s, 1);
		if (!best.empty())
			tail += ", najwyzej " + best;
		Append(out, tail + ".");
		g.reason = "Tak wybralem u trenera i tak juz zostalo.";
		return out;
	}

	inline std::string GenEmpire(TGen& g)
	{
		if (!*EmpireName(g.s.empire))
			return "Sam juz nie wiem, heh.";
		static const char* const k[] = { "Jestem z $EMPIRE.", "$EMPIRE.", "Z $EMPIRE, od zawsze." };
		return PBC_SAY(g, k);
	}

	inline std::string GenName(TGen& g)
	{
		if (g.tier >= TIER_FRIEND)
		{
			static const char* const k[] = { "Przeciez mnie znasz, $NAME :)", "No $NAME, a kto?" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Jestem $NAME.", "$NAME. Milo mi.", "$NAME, a ty to $PLAYER, prawda?" };
		return PBC_SAY(g, k);
	}

	inline std::string GenGoal(TGen& g)
	{
		static const char* const k[] = { "Ogolnie chce $G.", "Moj plan to $G.", "Na dluzsza mete chce $G.", "Glownie chce $G." };
		std::string out = Pick(g, k, 4);
		ReplaceAll(out, "$G", GoalPhrase(g));
		if (g.s.goal == G_HUNTING && !g.s.huntMob.empty())
			Append(out, Fill(g, "Zostalo mi $HUNTN sztuk: $HUNT."));
		static const char* const kWhy[V_COUNT] = {
			"Bo chce byc mocniejszy.", "Bo chce zobaczyc, co jest dalej.", "Bo to sie oplaci.",
			"Bo lubie wyzwania.", "Bo wtedy moge wiecej pomoc ekipie." };
		g.reason = kWhy[g.voice];
		return out;
	}

	inline std::string GenNextPlan(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.dead)
			return "Najpierw musze wstac :)";
		if (g.LowHp())
		{
			g.reason = "Bo mam malo HP.";
			return "Najpierw odpoczne, bo mam malo HP.";
		}
		if (s.bagCells > 0 && s.freeCells <= 2 && !s.inTown)
		{
			g.reason = "Bo mam pelne EQ.";
			return "Zaraz wracam do miasta, EQ mam pelne. Potem pewnie znowu na exp.";
		}
		if (s.action == A_TRAVEL && IsKnownMap(s.travelMap) && s.travelMap != s.mapIndex)
		{
			static const char* const k[] = { "Najpierw dojde $DEST, potem sie zobaczy.", "Jak dojde $DEST, to tam zostane na troche." };
			return PBC_SAY(g, k);
		}
		if (s.askerInParty && Fighting(g))
		{
			static const char* const k[] = { "Jak skonczymy tutaj, pewnie wracam do miasta.", "Jeszcze troche tu pobijemy, a potem sie zobaczy." };
			return PBC_SAY(g, k);
		}
		if (s.shopStanding)
			return "Postoje jeszcze troche ze straganem, a potem pewnie na exp.";
		if (s.fishing)
			return "Jeszcze troche polowie, potem zobaczymy.";
		if (Fighting(g) && s.goal == G_LEVEL)
		{
			static const char* const k[] = { "Dalej expic, moze zmienie spot, jak wbije poziom.", "Jeszcze troche tu, potem pewnie do miasta sprzedac drop." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Potem pewnie $G.", "Pozniej chce $G.", "Dalej? Chyba $G." };
		std::string out = Pick(g, k, 3);
		ReplaceAll(out, "$G", GoalPhrase(g));
		return out;
	}

	inline std::string GenHp(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out;
		if (s.dead)
			return "Nie zyje wlasnie, czekam az wstane.";
		if (s.hpPct < 15)
			out = "Bardzo malo, ledwo stoje.";
		else if (s.hpPct < 30)
			out = "Malo, zaraz musze sie podleczyc.";
		else if (s.hpPct < 60)
			out = Fill(g, "Moze byc, okolo $HP%.");
		else if (s.hpPct < 95)
		{
			static const char* const k[] = { "Dobrze, okolo $HP%.", "W porzadku, $HP%." };
			out = PBC_SAY(g, k);
		}
		else
		{
			static const char* const k[] = { "Pelne, jestem w formie.", "Full HP." };
			out = PBC_SAY(g, k);
		}
		if (s.spPct < 20)
			Append(out, "Gorzej z SP, prawie pusto.");
		return out;
	}

	inline std::string GenGold(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.tier <= TIER_STRANGER && s.gold >= 1000000)
		{
			static const char* const k[] = { "Wystarczy mi na potrzeby :)", "Troche jest, nie narzekam." };
			return PBC_SAY(g, k);
		}
		std::string out;
		if (s.gold < 10000)
			out = "Prawie nic, bieda :(";
		else if (s.gold < 1000000)
			out = Fill(g, "Troche drobnych, $GOLD.");
		else if (s.gold < 50000000)
		{
			static const char* const k[] = { "Mam $GOLD, nie narzekam.", "Okolo $GOLD." };
			out = PBC_SAY(g, k);
		}
		else
			out = Fill(g, "Sporo, $GOLD. Ale nie mow nikomu :)");
		if (g.voice == V_MERCHANT && g.rng.Chance(40))
			Append(out, "I caly czas obracam tym na targu.");
		g.reason = g.voice == V_MERCHANT ? "Bo handluje." : "Bo sprzedaje drop.";
		return out;
	}

	inline std::string GenHorse(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.horseLevel <= 0)
			return "Nie mam jeszcze konia.";
		if (s.riding)
		{
			static const char* const k[] = { "Jade teraz na nim, ma $HORSELVL poziom.", "Siedze na nim wlasnie. $HORSELVL poziom." };
			return PBC_SAY(g, k);
		}
		if (s.goal == G_HORSE)
			return Fill(g, "Mam, $HORSELVL poziom. Wlasnie go rozwijam.");
		static const char* const k[] = { "Mam, $HORSELVL poziom.", "Jest, poziom $HORSELVL." };
		return PBC_SAY(g, k);
	}

	inline std::string GenEquipment(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.a && g.a->concepts.Has(C_BONUS) && !s.weaponName.empty())
			return Fill(g, "Bonusow nie licze co do punktu. Mam $WEAPON +$WPLUS i $ARMOR +$APLUS, jakos to idzie.");
		if (s.weaponName.empty())
			return "Na razie bez porzadnej broni.";
		std::string out;
		if (!s.armorName.empty())
		{
			static const char* const k[] = { "Nosze $WEAPON +$WPLUS i $ARMOR +$APLUS.", "$WEAPON +$WPLUS, do tego $ARMOR +$APLUS." };
			out = PBC_SAY(g, k);
		}
		else
			out = Fill(g, "Mam $WEAPON +$WPLUS.");
		if (s.weaponPlus >= 7)
			Append(out, "Nie narzekam.");
		else if (s.weaponPlus <= 2 && g.rng.Chance(50))
			Append(out, "Trzeba by to ulepszyc.");
		return out;
	}

	inline std::string GenInventory(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out = s.bagSummary.empty() ? std::string("Nic ciekawego w EQ.") : "W EQ mam m.in. " + s.bagSummary + ".";
		if (s.bagCells > 0)
			Append(out, Fill(g, "Wolnych miejsc $FREE."));
		return out;
	}

	inline std::string GenInventorySpace(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.bagCells <= 0)
			return "Nie wiem, jeszcze nie ogarnalem plecaka.";
		if (s.freeCells == 0)
		{
			g.reason = "Bo za duzo dropu nazbieralem.";
			return "Zero, EQ pelne. Musze do miasta.";
		}
		if (s.freeCells <= 5)
			return Fill(g, "Malo, $FREE wolnych miejsc.");
		static const char* const k[] = { "Mam $FREE wolnych miejsc.", "Jeszcze $FREE miejsc wolnych, spokojnie." };
		return PBC_SAY(g, k);
	}

	inline std::string ShopWhere(const TGen& g);
	inline std::string SayMoney(long long v);

	inline std::string GenItemOwn(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
			return "Co konkretnie?";
		if (obj == "czas" || obj == "chwile" || obj == "moment" || obj == "chwilke")
			return g.Bad() ? "Troche mam, o co chodzi?" : "Mam chwile, o co chodzi?";
		if (obj == "racje" || obj == "racja")
			return "No wiem :)";
		if (obj == "ochote" || obj == "ochota")
			return "Na co?";
		if (obj == "pomysl" || obj == "pomysly")
			return "Moze pobijemy cos razem?";
		std::string name;
		unsigned int count = 0;
		if (g.world && g.world->FindItem(obj, name, count))
		{
			std::string out = "Tak, mam " + name;
			if (count > 1)
				out += " x" + ToString(count);
			out += ".";
			return out;
		}
		long long price = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
			return "W EQ nie, ale mam " + name + " na straganie " + ShopWhere(g) + " za " + SayMoney(price) + ".";
		static const char* const k[] = { "Nie, nie mam tego.", "Nie mam czegos takiego w EQ.", "Niestety nie." };
		return PBC_SAY(g, k);
	}

	inline std::string GenParty(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		g.saidParty = true;
		if (!s.inParty)
		{
			if (g.voice == V_SOCIAL && g.tier >= TIER_STRANGER && g.askBack.empty())
			{
				g.askBack = "Zagramy razem?";
				g.askBackKind = ASK_JOIN;
			}
			static const char* const k[] = { "Teraz jestem sam.", "Sam, nikt mnie nie zaprosil :)", "Solo na razie." };
			g.reason = g.voice == V_GRINDER ? "Bo sam szybciej expie." : "Bo nikt mnie nie zaprosil :)";
			return PBC_SAY(g, k);
		}
		if (s.askerInParty && !(g.a && g.a->concepts.Has(C_WHO)))
			return "No z toba przeciez :)";
		if (g.a && (g.a->follow == F_WHO || g.a->concepts.Has(C_WHO)))
			return Fill(g, "Liderem jest $LEADER.");
		if (s.leaderIsMe)
		{
			static const char* const k[] = { "Prowadze PT, jest nas $PARTYN.", "Mam swoje PT, $PARTYN osob." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Jestem w PT z $LEADER, razem $PARTYN osob.", "W grupie, $PARTYN osob. Lider to $LEADER." };
		return PBC_SAY(g, k);
	}

	inline std::string GenGuild(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (!s.inGuild)
		{
			static const char* const kSoc[] = { "Nie mam gildii. Szukam jakiejs fajnej.", "Jeszcze nie, ale chetnie bym do jakiejs wstapil." };
			static const char* const k[] = { "Nie mam gildii.", "Bez gildii na razie.", "Nie, jestem bez gildii." };
			g.reason = "Jakos nie trafilem na odpowiednia.";
			return g.voice == V_SOCIAL ? PBC_SAY(g, kSoc) : PBC_SAY(g, k);
		}
		std::string out;
		if (g.a && (g.a->follow == F_COUNT || g.a->concepts.Has(C_HOWMUCH) || g.a->concepts.Has(C_MANY)))
			out = Fill(g, "W $GUILD jest nas $GUILDN.");
		else
		{
			static const char* const k[] = { "Jestem w $GUILD, jest nas $GUILDN.", "$GUILD. Fajna ekipa.", "Tak, $GUILD." };
			out = PBC_SAY(g, k);
		}
		if (s.guildWar)
			Append(out, "Akurat mamy wojne!");
		return out;
	}

	inline std::string GenFishing(TGen& g)
	{
		if (g.s.fishing)
		{
			static const char* const k[] = { "Tak, lowie teraz ryby.", "No, siedze z wedka." };
			return PBC_SAY(g, k);
		}
		if (g.s.style == S_FISHER)
			return "Teraz nie, ale lubie polowic.";
		return "Nie, teraz nie lowie.";
	}

	inline std::string GenMining(TGen& g)
	{
		if (g.s.mining)
		{
			static const char* const k[] = { "Tak, kopie rude.", "No, kopie. Ciezka robota." };
			return PBC_SAY(g, k);
		}
		if (g.s.style == S_MINER)
			return "Teraz nie, ale kopanie to moja dzialka.";
		return "Nie, teraz nie kopie.";
	}

	inline std::string GenHerbalism(TGen& g)
	{
		return g.s.herbUnlocked ? "Tak, znam sie troche na ziolach." : "Nie, zielarstwa jeszcze nie ogarniam.";
	}

	inline std::string GenBiologist(TGen& g)
	{
		if (!g.s.bioWanted.empty())
			return Fill(g, g.s.action == A_BIOLOGIST ? "Wlasnie zbieram $BIO dla Biologa." : "Szukam teraz $BIO dla Biologa.");
		if (g.s.action == A_BIOLOGIST)
			return "Wlasnie robie jego zadanie.";
		return "Teraz nic dla Biologa nie zbieram.";
	}

	inline std::string GenMetin(TGen& g)
	{
		if (Fighting(g) && g.s.targetStone)
			return "Wlasnie jednego bije!";
		if (g.s.metinHunter || g.s.goal == G_METIN)
			return "Poluje na nie, jak tylko sie jakis pojawi.";
		if (g.voice == V_FIGHTER)
			return "Uwielbiam je bic, ale teraz akurat zadnego nie widze.";
		return "Teraz nie, ale jak jakis sie trafi, to bije.";
	}

	inline std::string GenDemonTower(TGen& g)
	{
		if (g.s.demonTower)
			return g.s.mapIndex == 66 ? "Tak, jestem w Wiezy Demonow." : "Tak, mam sprawy w Wiezy Demonow.";
		return g.s.level >= 40 ? "Teraz nie, moze kiedys z ekipa." : "Jeszcze za slaby jestem na Wieze.";
	}

	inline std::string GenGuildWar(TGen& g)
	{
		if (g.s.guildWar)
			return "Tak, mamy wojne gildii!";
		if (g.s.inGuild)
			return "Teraz spokoj, zadnej wojny.";
		return "Nie mam gildii, wiec i wojen nie ma.";
	}

	inline std::string GenMercenary(TGen& g)
	{
		if (g.s.mercContract)
			return "Tak, mam teraz kontrakt.";
		if (g.s.style == S_MERC)
			return "Teraz nie mam kontraktu. Ale jakby co, jestem do wynajecia.";
		return "Nie, nie mam teraz zadnego kontraktu.";
	}

	inline std::string GenPartyRequest(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.tier == TIER_HOSTILE)
			return "Nie, dzieki.";
		if (s.askerInParty)
			return "Przeciez juz jestesmy razem :)";
		if (g.a && g.a->follow == F_HOW)
			return "Po prostu zapros mnie do grupy.";
		if (s.inParty)
			return "Jestem teraz w innym PT, moze pozniej.";
		if (s.shopStanding)
			return "Teraz stoje ze straganem, moze pozniej.";
		if (s.dead || g.LowHp())
			return "Chwila, najpierw sie podlecze.";
		std::string out;
		if (s.askerLevel > 0 && (s.askerLevel > s.level + 15 || s.level > s.askerLevel + 15))
			out = "Chetnie, ale mamy duza roznice poziomow. Zapros, zobaczymy.";
		else if (g.tier == TIER_STRANGER)
		{
			static const char* const k[] = { "Nie znamy sie jeszcze, ale czemu nie. Zapros mnie do PT.", "Mozemy sprobowac. Zapros mnie, zobaczymy." };
			out = PBC_SAY(g, k);
		}
		else if (g.tier == TIER_KNOWN)
		{
			static const char* const k[] = { "Jasne, mozemy sprobowac. Zapros mnie.", "Jasne, zapros mnie do PT." };
			out = PBC_SAY(g, k);
		}
		else
		{
			static const char* const k[] = { "Z toba zawsze mozna isc. Dawaj zaproszenie.", "Jasne! Zapros mnie, juz ide." };
			out = PBC_SAY(g, k);
		}
		return out;
	}

	// ------------------------------------------------------------- trade

	inline std::string SayMoney(long long v)
	{
		return FormatYang(v);
	}

	inline std::string ShopWhere(const TGen& g)
	{
		std::string out = Fill(g, "$SHOPAT");
		if (g.s.shopOtherChannel)
			out += " (inny kanal)";
		return out;
	}

	// One line of the bot's own stall, and what to say about it - with a
	// player's offer weighed against the asking price when one was named.
	inline std::string ShopLineAnswer(TGen& g, const std::string& name, long long price, unsigned int count)
	{
		const long long offer = g.a ? g.a->offerYang : 0;
		std::string out;
		if (offer > 0 && price > 0)
		{
			if (offer > price + price / 20)
			{
				static const char* const k[] = {
					"Stoi nawet taniej, za $PRICE. Kup normalnie na straganie $WHERE.",
					"Nie musisz przeplacac, na straganie $WHERE stoi za $PRICE." };
				out = PBC_PICK(g, k);
			}
			else if (offer >= price)
			{
				static const char* const k[] = {
					"Za $OFFER moze byc. $ITEM stoi na moim straganie $WHERE, kup normalnie.",
					"$OFFER? Pasuje. Masz to na straganie $WHERE za $PRICE." };
				out = PBC_PICK(g, k);
			}
			else if (offer * 100 >= price * 80)
			{
				static const char* const k[] = {
					"Troche malo. Stoi za $PRICE, taniej raczej nie zejde.",
					"Blisko, ale stoi za $PRICE. Taniej nie oddam." };
				out = PBC_PICK(g, k);
			}
			else
			{
				static const char* const k[] = { "Za $OFFER? Nie, stoi za $PRICE.", "Za malo. Chce $PRICE." };
				out = PBC_PICK(g, k);
			}
		}
		else if (count > 1)
			out = "Tak, mam $ITEM x$COUNT na straganie $WHERE, $PRICE za calosc.";
		else
		{
			static const char* const k[] = { "Tak, mam $ITEM na straganie $WHERE za $PRICE.", "Mam. $ITEM, $PRICE, stragan $WHERE." };
			out = PBC_PICK(g, k);
		}
		ReplaceAll(out, "$OFFER", SayMoney(offer));
		ReplaceAll(out, "$PRICE", SayMoney(price));
		ReplaceAll(out, "$COUNT", ToString(count));
		ReplaceAll(out, "$WHERE", ShopWhere(g));
		ReplaceAll(out, "$ITEM", name);
		CapitalizeFirst(out);
		return out;
	}

	// "masz na straganie fms?", "sprzedasz mi 12d za 5kk?"
	inline std::string ShopItemAnswer(TGen& g, const std::string& obj)
	{
		std::string name;
		long long price = 0;
		unsigned int count = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
			return ShopLineAnswer(g, name, price, count);
		if (g.s.shopOpen)
		{
			std::string out = "Na straganie tego nie mam.";
			if (!g.s.shopSummary.empty())
				Append(out, "Mam za to: " + g.s.shopSummary + ".");
			return out;
		}
		unsigned int bagCount = 0;
		if (g.world && g.world->FindItem(obj, name, bagCount))
			return "Straganu teraz nie mam, ale " + name + " mam w EQ.";
		return "Nie mam tego, a straganu teraz tez nie.";
	}

	inline std::string GenShop(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (g.a && !g.a->object.empty())
			return ShopItemAnswer(g, g.a->object);
		if (s.shopOpen && !s.shopSummary.empty())
		{
			std::string out = "Mam stragan " + ShopWhere(g) + ". Na nim m.in.: " + s.shopSummary + ".";
			if (s.shopItems > 3)
				Append(out, "I jeszcze troche innych rzeczy.");
			return out;
		}
		if (s.shopOpen)
			return "Mam stragan, ale juz prawie wszystko zeszlo.";
		if (g.voice == V_MERCHANT)
			return "Teraz nie mam straganu, ale niedlugo cos wystawie.";
		return "Nie mam teraz straganu.";
	}

	inline std::string GenPrice(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
		{
			if (g.s.shopOpen && !g.s.shopSummary.empty())
				return "U mnie: " + g.s.shopSummary + ".";
			return "Ale czego cena?";
		}
		std::string name;
		long long price = 0;
		unsigned int count = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
		{
			if (g.a->offerYang > 0)
				return ShopLineAnswer(g, name, price, count);
			std::string out = "U mnie na straganie " + name + " stoi za " + SayMoney(price) + ".";
			if (count > 1)
				out = "U mnie " + name + " x" + ToString(count) + " za " + SayMoney(price) + " calosc.";
			return out;
		}
		unsigned int sellers = 0;
		if (g.world && g.world->FindMarketPrice(obj, name, price, sellers))
		{
			static const char* const k[] = {
				"Na targu widzialem $ITEM po $PRICE.", "$ITEM chodzi teraz po jakies $PRICE.",
				"Najtaniej widzialem $ITEM za $PRICE." };
			std::string out = PBC_PICK(g, k);
			ReplaceAll(out, "$ITEM", name);
			ReplaceAll(out, "$PRICE", SayMoney(price));
			CapitalizeFirst(out);
			if (g.voice == V_MERCHANT && g.rng.Chance(50))
				Append(out, "Ceny sie jednak zmieniaja.");
			return out;
		}
		static const char* const k[] = {
			"Nie wiem, dawno nie widzialem tego na targu.", "Ciezko powiedziec, nikt tego ostatnio nie wystawial." };
		return PBC_SAY(g, k);
	}

	inline std::string GenMarket(TGen& g)
	{
		if (g.s.marketTrip || g.s.action == A_MARKET)
			return "Wlasnie ide na targ, zobacze co jest.";
		if (g.voice == V_MERCHANT)
		{
			static const char* const k[] = { "Ceny ostatnio skacza, trzeba uwazac.", "Handel to moja dzialka. Kupuje tanio, sprzedaje drozej." };
			return PBC_SAY(g, k);
		}
		return "Handluje troche, jak mam cos na zbyciu.";
	}

	inline std::string GenBuy(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
			return g.s.shopOpen && !g.s.shopSummary.empty()
					? "Na straganie mam: " + g.s.shopSummary + ". Co cie interesuje?"
					: std::string("Co konkretnie chcesz kupic?");
		std::string name;
		long long price = 0;
		unsigned int count = 0;
		if (g.world && g.world->FindShopItem(obj, name, price, count))
			return ShopLineAnswer(g, name, price, count);
		if (g.world && g.world->FindItem(obj, name, count))
			return "Mam " + name + " w EQ, ale nie wystawilem tego na sprzedaz.";
		if (g.s.shopOpen)
			return "Tego nie mam na straganie.";
		return "Nie mam tego teraz na sprzedaz.";
	}

	inline std::string GenSell(TGen& g)
	{
		const std::string obj = g.a ? g.a->object : std::string();
		if (obj.empty())
			return "Co chcesz mi sprzedac?";
		std::string r = g.world ? g.world->AnswerSell(obj) : std::string();
		return r.empty() ? std::string("Nie potrzebuje teraz tego.") : r;
	}

	inline std::string GenSkills(TGen& g)
	{
		std::string out;
		if (g.s.action == A_READ_BOOK)
			out = "Wlasnie czytam ksiegi, podbijam skille.";
		else if (g.s.goal == G_SKILL)
			out = "Teraz glownie podbijam umiejetnosci.";
		else
		{
			static const char* const k[] = { "Rozwijam, jak mam ksiegi. Tanio nie jest.", "Powoli do przodu z umiejetnosciami." };
			out = PBC_SAY(g, k);
		}
		// What they are at, from the character sheet.
		const std::string list = SkillsList(g.s, 3);
		if (!list.empty())
			Append(out, "Najwyzej mam " + list + ".");
		return out;
	}

	inline std::string GenPvp(TGen& g)
	{
		if (g.LowHp() || g.s.dead)
			return "Nie teraz, mam malo HP.";
		if (g.voice == V_FIGHTER)
			return "Chetnie! Wyzwij mnie normalnie przez PvP.";
		return "Mozemy, ale wyzwij mnie normalnie przez PvP.";
	}

	inline std::string GenTravel(TGen& g)
	{
		if (const long mentioned = MentionedMap(g))
		{
			if (mentioned == g.s.mapIndex && IsKnownMap(g.s.mapIndex))
			{
				g.saidMap = true;
				return Fill(g, "Juz jestem $MAPIN.");
			}
			if (g.s.action == A_TRAVEL && mentioned == g.s.travelMap)
				return Fill(g, "Tak, ide $DEST.");
			std::string out = "Nie. ";
			out += ActivityClause(g, true);
			g.saidMap = true;
			return out;
		}
		if (g.s.action == A_TRAVEL && IsKnownMap(g.s.travelMap) && g.s.travelMap != g.s.mapIndex)
		{
			g.saidMap = true;
			return ActivityClause(g, true);
		}
		if (IsKnownMap(g.s.mapIndex))
		{
			g.saidMap = true;
			return Fill(g, "Nigdzie, zostaje $MAPIN.");
		}
		return "Nigdzie, zostaje tutaj.";
	}

	inline std::string GenRest(TGen& g)
	{
		if (g.s.action == A_RECOVER || g.s.action == A_TOWN_REST)
			return "Tak, odpoczywam chwile.";
		if (g.LowHp())
			return "Zaraz bede musial, HP mi siada.";
		return "Nie, jeszcze mam sile.";
	}

	inline std::string GenRefine(TGen& g)
	{
		if (g.s.action == A_REFINE)
			return "Wlasnie ulepszam, trzymaj kciuki.";
		if (g.s.euphoria)
			return "Ostatnio weszlo mi niezle ulepszenie!";
		if (g.s.goal == G_REFINE)
			return "Planuje ulepszyc bron, jak tylko zbiore materialy.";
		if (!g.s.weaponName.empty())
			return Fill(g, "Ulepszam, jak mam materialy. Teraz mam $WEAPON +$WPLUS.");
		return "Jak bedzie z czego, to ulepsze.";
	}

	inline std::string GenMissions(TGen& g)
	{
		if (!g.s.huntMob.empty())
			return Fill(g, "Mam polowanie na $HUNT, zostalo $HUNTN.");
		if (!g.s.bioWanted.empty())
			return Fill(g, "Zbieram $BIO dla Biologa.");
		return "Teraz zadnej misji nie mam.";
	}

	inline std::string GenDeath(TGen& g)
	{
		if (g.s.dead)
			return "No wlasnie leze, ktos mnie ubil.";
		if (g.s.recentDeaths > 0 && g.s.minutesSinceDeath < 60)
			return g.s.recentDeaths > 1 ? "Kilka razy juz dzis padlem. Bywa." : "Tak, niedawno zginalem. Bywa.";
		return "Nie, dzis jeszcze nie :)";
	}

	inline std::string GenRelationship(TGen& g)
	{
		switch (g.tier)
		{
			case TIER_HOSTILE: return "Szczerze? Nie bardzo po tym, co pisales.";
			case TIER_STRANGER: return "Dopiero sie poznajemy, ale wydajesz sie spoko.";
			case TIER_KNOWN: return "Jasne, spoko jestes.";
			case TIER_FRIEND: return "Pewnie! Dobrze sie z toba gada.";
			default: return "No jasne, jestes jednym z moich ulubionych ludzi tutaj :)";
		}
	}

	inline std::string GenTimeHere(TGen& g)
	{
		const u32 m = g.s.actionMinutes > 0 ? g.s.actionMinutes : g.s.goalMinutes;
		if (m >= 60)
			return Fill(g, "Dluzsza chwile, ponad godzine.");
		if (m >= 10)
			return "Z kilkanascie minut, moze wiecej.";
		if (m > 0)
			return "Dopiero przyszedlem.";
		return "Nie liczylem, chwile.";
	}

	inline std::string GenMapOpinion(TGen& g)
	{
		const long mentioned = MentionedMap(g);
		if (mentioned && mentioned != g.s.mapIndex)
		{
			const TMapWords& w = GetMapWords(mentioned);
			const int roll = OpinionRoll(g, *w.name ? w.name : "gdzies", 23);
			std::string place = *w.name ? w.name : "Tam";
			if (roll < 45)
				return place + "? Lubie, dobre miejsce.";
			if (roll < 80)
				return place + "? Moze byc, zalezy na co.";
			return place + "? Srednio, wole inne miejsca.";
		}
		if (g.s.inTown)
			return "Miasto jak miasto. Lubie tu wrocic po expie.";
		if (!IsKnownMap(g.s.mapIndex))
			return "Dziwne miejsce, ale moze byc.";
		static const char* const k[V_COUNT][2] = {
			{ "$MAPNAME jest ok, exp leci.", "Lubie, jesli exp dobry." },
			{ "Lubie, ladnie tu.", "$MAPNAME ma swoj klimat." },
			{ "Moze byc, drop sie dobrze sprzedaje.", "Jest ok, jak cos wypada." },
			{ "Jest ok, byle bylo z czym walczyc.", "Lubie, tu sa mocne moby." },
			{ "Lubie, czesto kogos tu spotykam.", "Fajnie, zwlaszcza z ekipa." },
		};
		return Say(g, k[g.voice], 2);
	}

	inline std::string GenDropLuck(TGen& g)
	{
		if (g.s.unlucky)
			return "Kiepsko, dawno nic dobrego nie wypadlo.";
		if (g.s.euphoria)
			return "Swietnie! Ostatnio mialem farta.";
		if (g.Good())
			return "Calkiem niezle, nie narzekam.";
		static const char* const k[] = { "Normalnie, nic specjalnego.", "Jak zwykle, raz lepiej, raz gorzej." };
		return PBC_SAY(g, k);
	}

	inline std::string GenProgressToday(TGen& g)
	{
		if (g.s.onlineMinutes >= 120)
			return "Troche sie zrobilo, gram juz dobrych kilka godzin.";
		if (g.s.onlineMinutes >= 30)
			return "Troche expa wpadlo, gram od jakiegos czasu.";
		return "Dopiero zaczalem, jeszcze nic wielkiego.";
	}

	inline std::string GenPersonality(TGen& g)
	{
		static const char* const k[V_COUNT][2] = {
			{ "Lubie konkret. Exp, poziom i dalej.", "Raczej malo gadam, wiecej expie." },
			{ "Lubie pochodzic, pozwiedzac i pogadac.", "Ciekawy swiata, tak bym powiedzial." },
			{ "Bardziej handlarz niz wojownik.", "Lubie dobre interesy." },
			{ "Lubie dobra walke, im mocniej, tym lepiej.", "Raczej nie uciekam od ryzyka." },
			{ "Lubie grac z ludzmi. Samemu jest nudno.", "Towarzyski jestem, chyba to widac." },
		};
		return Say(g, k[g.voice], 2);
	}

	inline std::string GenMood(TGen& g)
	{
		if (g.s.dead)
			return "Kiepsko, wlasnie zginalem.";
		if (g.Good())
			return g.LowHp() ? "Humor dobry, tylko HP mniej :)" : "Swietny! Wszystko idzie jak trzeba.";
		if (g.Bad())
		{
			g.reason = g.s.unlucky ? "Bo dawno nic dobrego nie wypadlo." : "Jakos nic nie idzie.";
			return g.s.unlucky ? "Kiepski. Pech mnie przesladuje." : "Kiepski, szczerze mowiac.";
		}
		return "Normalnie, spokojnie.";
	}

	// --------------------------------------------------------------- social

	inline std::string GenGreeting(TGen& g, bool shortForm)
	{
		const bool again = g.m.greetedAt != 0 && g.now - g.m.greetedAt < CONV_SESSION_GAP_MS;
		g.m.greetedAt = g.now;
		g.saidGreeting = true;
		if (again)
		{
			static const char* const k[] = { "No siema jeszcze raz :)", "Hej hej.", "Juz sie witalismy :)" };
			return PBC_SAY(g, k);
		}
		if (shortForm)
		{
			static const char* const k[] = { "Hej!", "Siema!", "Czesc!" };
			return PBC_SAY(g, k);
		}
		const bool longGap = g.a && g.a->gapBefore > 6u * 3600u * 1000u;
		if (g.tier >= TIER_FRIEND)
		{
			if (longGap)
				return Fill(g, "Siema $PLAYER! Dawno cie nie bylo.");
			static const char* const k[] = { "O, siema $PLAYER!", "Hej $PLAYER! Co tam?", "Siemano $PLAYER." };
			return PBC_SAY(g, k);
		}
		if (g.tier == TIER_KNOWN)
		{
			static const char* const k[] = { "O, siema $PLAYER.", "Hej $PLAYER.", "Czesc, co tam?" };
			return PBC_SAY(g, k);
		}
		if (g.tier == TIER_HOSTILE)
			return "No.";
		static const char* const kNight[] = { "Hej. Pozno juz, a ty dalej grasz?", "Siema. Nocna zmiana?" };
		if (g.s.hour >= 0 && g.s.hour < 5 && g.rng.Chance(40))
			return PBC_SAY(g, kNight);
		static const char* const k[] = { "Hej.", "Siema!", "Czesc.", "Hej, co tam?" };
		return PBC_SAY(g, k);
	}

	inline std::string GenFarewell(TGen& g)
	{
		if (g.tier >= TIER_FRIEND)
		{
			static const char* const k[] = { "Nara $PLAYER, do nastepnego!", "Trzymaj sie $PLAYER!" };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Na razie!", "Trzymaj sie.", "Do zobaczenia.", "Powodzenia na expie." };
		return PBC_SAY(g, k);
	}

	inline std::string GenHowAreYou(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		std::string out;
		if (s.dead)
			out = "Kiepsko, wlasnie zginalem :(";
		else if (g.LowHp())
			out = "Moglo byc lepiej, mam malo HP.";
		else if (g.Bad())
		{
			static const char* const k[] = { "Tak sobie. Dzis jakos nie idzie.", "Srednio, szczerze mowiac." };
			out = PBC_SAY(g, k);
		}
		else if (g.Good() || s.euphoria)
		{
			static const char* const k[] = { "Swietnie! Wszystko idzie jak trzeba.", "Super, dzis mi wszystko wychodzi." };
			out = PBC_SAY(g, k);
		}
		else
		{
			static const char* const k[] = { "Spoko, robie swoje.", "W porzadku.", "Dobrze, nie narzekam." };
			out = PBC_SAY(g, k);
			if (!g.saidActivity && !g.groupHasActivity && g.rng.Chance(60))
			{
				const std::string act = ActivityClause(g, !g.saidMap);
				if (IsKnownMap(s.mapIndex) && act.find(GetMapWords(s.mapIndex).atShort) != std::string::npos)
					g.saidMap = true;
				Append(out, act);
				g.saidActivity = true;
			}
		}
		if (g.askBack.empty() && !g.Bad() && g.tier != TIER_HOSTILE && g.rng.Chance(g.voice == V_SOCIAL ? 70 : 45))
		{
			static const char* const k[] = { "A u ciebie?", "A ty jak?", "A co u ciebie?" };
			g.askBack = PBC_SAY(g, k);
			g.askBackKind = ASK_HOW_ARE_YOU;
		}
		return out;
	}

	inline std::string GenHelp(TGen& g)
	{
		(void)g;
		return "Pytaj normalnie, jak czlowieka :) Moge powiedziec co robie, gdzie jestem, ile jest mobow, jaki mam lvl, EQ, gildie, PT i plany. A jak chcesz, pogadamy o czymkolwiek.";
	}

	inline std::string GenIsBot(TGen& g)
	{
		static const char* const k[] = { "A co, tak slabo gram? xD", "Bot to ty jestes :P", "Hehe, gram po prostu duzo." };
		return PBC_SAY(g, k);
	}

	inline std::string GenInsult(TGen& g)
	{
		if (g.m.negative >= 4)
			return std::string(); // stops answering an abusive line
		if (g.m.negative >= 3)
			return "Nie mam ochoty tak rozmawiac.";
		static const char* const k[] = { "Spokojnie, bez nerwow.", "Nie musisz tak od razu.", "Ok, jak uwazasz." };
		return PBC_SAY(g, k);
	}

	inline std::string GenPraise(TGen& g)
	{
		static const char* const k[] = { "Dzieki! Ty tez spoko.", "Hehe, dzieki.", "Milo slyszec." };
		return PBC_SAY(g, k);
	}

	inline std::string GenAge(TGen& g)
	{
		static const char* const k[] = { "Wystarczajaco, zeby grac do rana :)", "O wieku sie nie rozmawia, hehe.", "A co, wygladam staro?" };
		return PBC_SAY(g, k);
	}

	inline std::string GenOrigin(TGen& g)
	{
		if (!*EmpireName(g.s.empire))
			return "Stad i stamtad.";
		static const char* const k[] = { "Z $EMPIRE.", "Jestem z $EMPIRE. A ty?", "$EMPIRE, od urodzenia." };
		return PBC_SAY(g, k);
	}

	inline std::string GenItemShop(TGen& g)
	{
		if (!g.s.dragonKnown)
			return "Nie sprawdzalem ostatnio, ile mam SM.";
		if (g.s.dragonCoins <= 0)
			return g.voice == V_MERCHANT ? "Zero SM. Wole yang, IS to nie moja bajka." : "Zero SM, wszystko wydalem.";
		static const char* const k[] = { "Mam $SM SM.", "Jakies $SM SM, nie wiecej." };
		return PBC_SAY(g, k);
	}

	inline std::string GenKs(TGen& g)
	{
		if (g.tier == TIER_HOSTILE)
			return "Nie widzialem tam twojego imienia.";
		static const char* const k[] = {
			"Sorki, nie zauwazylem, ze go bijesz.", "Oj, wybacz, nie widzialem ciebie.", "Sorry, nie chcialem ci ksowac." };
		return PBC_SAY(g, k);
	}

	inline std::string GenReady(TGen& g)
	{
		if (g.s.dead)
			return "Chwila, jeszcze leze.";
		if (g.LowHp())
			return "Chwila, najpierw sie podlecze.";
		static const char* const k[] = { "Gotowy!", "Jasne, rdy.", "Moge isc." };
		return PBC_SAY(g, k);
	}

	inline std::string GenGoodLuck(TGen& g)
	{
		static const char* const k[] = { "Dzieki, tobie tez!", "Nawzajem!", "Dzieki, przyda sie." };
		return PBC_SAY(g, k);
	}

	inline std::string GenBrb(TGen& g)
	{
		static const char* const k[] = { "Jasne, czekam.", "Ok, bede tu.", "Spoko." };
		return PBC_SAY(g, k);
	}

	// ------------------------------------------------------------- buffs

	inline std::string DurationText(int seconds)
	{
		if (seconds < 120)
			return ToString(seconds) + " s";
		const int minutes = seconds / 60;
		const int rest = seconds % 60;
		return rest ? ToString(minutes) + " min " + ToString(rest) + " s" : ToString(minutes) + " min";
	}

	// What one buff does, in the words a player reads it in. The numbers are
	// the engine's (TBuffLine); only the words are chosen here.
	inline std::string BuffEffectText(const TBuffLine& l)
	{
		char buf[160];
		buf[0] = 0;
		switch (l.skill)
		{
			case CONV_SKILL_BLESSING:
				snprintf(buf, sizeof(buf), "o %d%% mniej obrazen", l.amount);
				break;
			case CONV_SKILL_REFLECT:
				snprintf(buf, sizeof(buf), "odbija %d%% obrazen wrecz", l.amount);
				break;
			case CONV_SKILL_DRAGON_AID:
				snprintf(buf, sizeof(buf), "+%d%% szansy na cios krytyczny", l.amount);
				break;
			case CONV_SKILL_CURE:
				if (l.amountMax > l.amount)
					snprintf(buf, sizeof(buf), "leczy %d-%d HP", l.amount, l.amountMax);
				else
					snprintf(buf, sizeof(buf), "leczy %d HP", l.amount);
				break;
			case CONV_SKILL_SWIFTNESS:
				if (l.amount2 > 0)
					snprintf(buf, sizeof(buf), "+%d do szybkosci ruchu i +%d%% do szybkosci czarowania",
							l.amount, l.amount2);
				else
					snprintf(buf, sizeof(buf), "+%d do szybkosci ruchu", l.amount);
				break;
			case CONV_SKILL_ATTACK_UP:
				snprintf(buf, sizeof(buf), "+%d do wartosci ataku", l.amount);
				break;
			default:
				break;
		}
		std::string out = buf;
		if (l.seconds > 0)
			out += " przez " + DurationText(l.seconds);
		if (l.skill == CONV_SKILL_CURE && l.amount3 > 0)
		{
			out += ", do tego oslona na " + ToString(l.amount3) + " obrazen od potworow";
			if (l.seconds3 > 0)
				out += " przez " + DurationText(l.seconds3);
		}
		return out;
	}

	// "zbuffuj mnie", "dasz buffa?" - asked for, not asked about.
	inline bool IsBuffRequest(const TAnalysis& a)
	{
		for (size_t i = 0; i < a.tokens.words.size(); ++i)
		{
			const std::string& w = a.tokens.words[i];
			if (StartsWith(w, "zbuf") || StartsWith(w, "buffuj") || StartsWith(w, "bufuj") ||
					StartsWith(w, "buffnij") || StartsWith(w, "bufnij"))
				return true;
		}
		return a.concepts.Has(C_BUFF) && (a.tokens.Has("daj") || a.tokens.Has("dasz") ||
				a.tokens.Has("potrzebuje") || (a.concepts.Has(C_CAN) && a.concepts.Has(C_ME)));
	}

	// "co daja twoje buffy?", "ile daje blogoslawienstwo?" - the Shaman's
	// buffs of its path as the engine would cast them on the person asking.
	inline std::string GenBuffs(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		int askedWord = -1;
		const unsigned int asked = g.a && g.a->concepts.Has(C_BUFFNAME) ? NamedBuffSkill(g.a->tokens, askedWord) : 0;
		g.reason = "Tyle wychodzi z moich skilli i inteligencji.";
		if (s.job != 3)
		{
			std::string out = std::string("Nie mam buffow, gram ") + ClassNameInstr(s.job) + ".";
			Append(out, "Buffy daje szaman.");
			return out;
		}
		const int build = s.Build();
		if (build == B_NONE)
			return "Nie wybralem jeszcze sciezki, wiec buffow jeszcze nie mam.";
		if (asked && BuffBuildOf(asked) != build)
			return std::string("Tego nie mam, jestem ") + BuildPhrase(build) + ". " + SkillNameOf(asked) +
					" ma szaman " + BuildShortName(BuffBuildOf(asked)) + ".";
		TBuffReport report;
		if (!g.world || !g.world->DescribeBuffs(report) || report.count <= 0)
			return "Nie umiem ci teraz tego policzyc.";
		std::string list;
		int shown = 0;
		int unlearnt = 0;
		for (int i = 0; i < report.count && i < 3; ++i)
		{
			const TBuffLine& l = report.lines[i];
			if (asked && l.skill != asked)
				continue;
			if (l.level <= 0)
			{
				if (asked)
					return std::string("Tego jeszcze sie nie nauczylem (") + SkillNameOf(l.skill) + ").";
				++unlearnt;
				continue;
			}
			if (!l.known)
				continue;
			Append(list, std::string(SkillNameOf(l.skill)) + " (" + SkillGradeText(l.level) + ") - " +
					BuffEffectText(l) + ".");
			++shown;
		}
		if (shown == 0)
			return unlearnt ? "Zadnego buffa jeszcze sie nie nauczylem." : "Nie umiem ci teraz tego policzyc.";
		std::string out = std::string(report.onAsker ? "Na tobie: " : "Moje buffy: ") + list;
		if (unlearnt)
			Append(out, "Reszty jeszcze nie umiem.");
		if (g.a && IsBuffRequest(*g.a))
			Append(out, s.askerInParty ? "Jestesmy w PT, to pilnuje twoich buffow." :
					"Zapros mnie do PT, to bede cie buffowac.");
		return out;
	}

	// --------------------------------------------------------- coming over

	// Whether the line gives a reason a stranger would come for.
	inline bool SummonHasReason(const TAnalysis& a)
	{
		static const int kReasons[] = {
			C_EXP, C_HIT, C_MOB, C_METIN, C_BOSS, C_DT, C_HELP, C_PARTY, C_TRADE, C_SHOP, C_BUYME,
			C_SELLYOU, C_GIVE, C_QUEST, C_BIO, C_WAR, C_PVP, C_BUFF, C_BUFFNAME, C_FISH, C_MINE, C_ITEMWORD,
			C_GEAR, C_GOLD, C_UPGRADE, C_GUILD, C_HORSE };
		for (size_t i = 0; i < sizeof(kReasons) / sizeof(kReasons[0]); ++i)
			if (a.concepts.Has(kReasons[i]))
				return true;
		// "chodz" is a joining word too, and the one the summon itself is said
		// with: only another one ("razem", "zaprosze", "dolacz") is a reason.
		if (a.concepts.Has(C_JOIN))
		{
			const int w = a.concepts.firstWord[C_JOIN];
			const std::string word = w >= 0 && w < (int)a.tokens.words.size() ? a.tokens.words[w] : std::string();
			if (word != "chodz" && word != "choc" && word != "chodzze")
				return true;
		}
		static const char* const kWords[] = {
			"dam", "dac", "dostaniesz", "prezent", "pokaze", "pokazac", "pomoc", "pomoz", "pomozesz",
			"potrzebuje", "sprawa", "sprawe", "pogadac", "porozmawiac", "zobaczysz", "zobacz" };
		for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); ++i)
			if (a.tokens.Has(kWords[i]))
				return true;
		return a.offerYang > 0;
	}

	// The same answer to the same stranger: the pair decides, not a roll per
	// line, or asking three times would be the way round a refusal.
	inline int SummonStrangerRoll(const TGen& g)
	{
		return (int)(HashStr("summon", g.m.botPID * 2654435761u ^ (g.m.playerPID * 40503u)) % 100u);
	}

	// How many strangers a voice sends away without asking what for.
	inline int SummonRefuseShare(const TGen& g)
	{
		int share = 30;
		switch (g.voice)
		{
			case V_SOCIAL: share = 10; break;
			case V_WANDERER: share = 25; break;
			case V_GRINDER: share = 45; break;
			default: break;
		}
		if (g.Bad())
			share += 20;
		return share;
	}

	inline std::string SummonRefusal(TGen& g, int block)
	{
		switch (block)
		{
			case SB_OTHER_MAP:
				g.reason = "Bo jestem na innej mapie.";
				if (IsKnownMap(g.s.mapIndex))
					return Fill(g, "Jestem daleko, $MAPIN. Stad nie dam rady przyjsc.");
				return "Jestem daleko, na innej mapie. Stad nie dam rady przyjsc.";
			case SB_STALL:
				g.reason = "Bo pilnuje straganu.";
				return "Stoje teraz ze straganem, nie moge odejsc.";
			case SB_FISHING:
				g.reason = "Bo lowie.";
				return "Wlasnie lowie, nie zostawie wedki.";
			case SB_MINING:
				g.reason = "Bo kopie rude.";
				return "Kopie teraz rude, nie moge odejsc.";
			case SB_DUEL:
				g.reason = "Bo mam pojedynek.";
				return "Mam teraz pojedynek, pozniej.";
			case SB_GUILD_WAR:
				g.reason = "Bo moja gildia ma wojne.";
				return "Moja gildia ma teraz wojne, nie moge.";
			case SB_TOWER:
				g.reason = "Bo jestem w Wiezy Demonow.";
				return "Jestem z gildia w Wiezy Demonow, teraz nie wyjde.";
			case SB_DUNGEON:
				g.reason = "Bo jestem w lochu.";
				return "Jestem w lochu, teraz stad nie wyjde.";
			case SB_MERC:
				g.reason = "Bo mam kontrakt.";
				return "Mam kontrakt, najpierw musze go skonczyc.";
			case SB_OTHER_PARTY:
				g.reason = "Bo jestem z kims innym.";
				return "Jestem teraz z kims w druzynie, nie moge odejsc.";
			case SB_OTHER_SUMMON:
				g.reason = "Bo juz ide do kogos innego.";
				return "Juz ide do kogos innego, sorki.";
			case SB_DEAD:
				return "Chwila, najpierw wstane.";
			default:
				return "Teraz nie moge, sorki.";
		}
	}

	// The walk begins - or the engine, asked once more, says no.
	inline std::string SummonGo(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.summonBlock != SB_NONE)
			return SummonRefusal(g, s.summonBlock);
		const int code = g.world ? g.world->StartSummon() : (int)SUMMON_START_FAILED;
		g.reason = "Bo mnie zawolales.";
		switch (code)
		{
			case SUMMON_START_OK:
			case SUMMON_START_RENEWED:
				if (s.askerOnMap && s.askerDistance >= 0 && s.askerDistance <= CONV_SUMMON_NEAR_DISTANCE)
					return "Jestem obok, poczekam chwile.";
				if (g.tier >= TIER_FRIEND)
				{
					static const char* const k[] = { "Jasne, juz lece!", "Dla ciebie zawsze, juz ide!" };
					return PBC_SAY(g, k);
				}
				{
					static const char* const k[] = { "Juz ide!", "Dobra, zaraz bede.", "Ok, ide do ciebie." };
					return PBC_SAY(g, k);
				}
			case SUMMON_START_BLOCKED:
				return "Teraz nie moge, sorki.";
			default:
				return "Nie widze cie, gdzie jestes?";
		}
	}

	// "chodz do mnie", "przyjdz", "podejdz". Somebody the bot knows it comes
	// to; a stranger is asked what for (or, by the pair's own roll, sent away);
	// what the bot cannot leave it says it cannot leave.
	inline std::string GenSummon(TGen& g)
	{
		const TBotSnapshot& s = g.s;
		if (s.summonedByAsker)
		{
			const int code = g.world ? g.world->StartSummon() : (int)SUMMON_START_FAILED;
			g.reason = "Bo mnie zawolales.";
			if (code == SUMMON_START_OK || code == SUMMON_START_RENEWED)
				return s.summonArrived ? "Przeciez jestem obok :) Zostane jeszcze chwile." : "Juz ide, juz!";
			return "Teraz nie moge, sorki.";
		}
		if (g.tier == TIER_HOSTILE)
		{
			g.reason = "Bo mnie obrazasz.";
			return "Po tym, jak mnie traktujesz? Nie.";
		}
		if (s.summonBlock != SB_NONE)
			return SummonRefusal(g, s.summonBlock);
		if (g.tier == TIER_STRANGER && !(g.a && SummonHasReason(*g.a)))
		{
			const bool askedAlready = g.m.botAsk == ASK_SUMMON && g.now - g.m.botAskAt < CONV_BOT_ASK_TTL_MS;
			if (askedAlready || SummonStrangerRoll(g) < SummonRefuseShare(g))
			{
				g.reason = "Bo sie nie znamy.";
				static const char* const k[] = {
					"Nie znamy sie, a ja mam swoje sprawy.", "Sorki, nie chodze do obcych bez powodu." };
				return PBC_SAY(g, k);
			}
			// The question belongs to this reply and outranks an "a ty?" another
			// line of the same batch may have put there: without it the answer
			// is not read as one, and a stranger with no reason must not be
			// walked to by default.
			g.askBack = "Po co mam przyjsc?";
			g.askBackKind = ASK_SUMMON;
			g.askBackTopic = T_NONE;
			return "Hm, nie znamy sie.";
		}
		return SummonGo(g);
	}

	// "mozesz isc", "wracaj do siebie".
	inline std::string GenDismiss(TGen& g)
	{
		if (g.s.summonedByAsker)
		{
			if (g.world)
				g.world->EndSummon();
			static const char* const k[] = {
				"Dobra, to wracam do swoich spraw.", "Ok, to lece. Na razie!", "Jasne. Gdyby co, pisz." };
			return PBC_SAY(g, k);
		}
		static const char* const k[] = { "Przeciez nigdzie za toba nie chodze :)", "Dobra, i tak mam swoje sprawy." };
		return PBC_SAY(g, k);
	}

	// Thanks, goodbye or an insult from the person who called the bot over
	// ends the stay as well.
	inline bool ReleaseSummonFor(TGen& g)
	{
		if (!g.s.summonedByAsker || !g.world)
			return false;
		g.world->EndSummon();
		return true;
	}

	// ------------------------------------------------------------- follow-ups

	inline std::string GenerateOne(TGen& g, const TAnalysis& a);

	inline std::string GenWhy(TGen& g, EIntent subject)
	{
		if (!g.m.lastReason.empty() && g.now - g.m.lastAnsweredAt < CONV_CONTEXT_TTL_MS)
			return g.m.lastReason;
		switch (subject)
		{
			case I_ACTIVITY: case I_ACTIVITY_LOCATION: case I_LOCATION: case I_TARGET: case I_MOB_COUNT:
				if (Fighting(g))
				{
					static const char* const kWhy[V_COUNT] = {
						"Bo tu jest dobry exp na moj poziom.", "Bo akurat tu trafilem i jest spokojnie.",
						"Bo z tych mobow leci cos, co sie sprzedaje.", "Bo lubie walke, a tu jest z kim.",
						"Bo tu zawsze ktos jest." };
					return kWhy[g.voice];
				}
				if (g.s.inTown && g.s.bagCells > 0 && g.s.freeCells <= 5)
					return "Bo mialem pelne EQ i trzeba bylo sprzedac.";
				if (g.s.action == A_RECOVER || g.s.action == A_TOWN_REST)
					return "Bo mialem malo HP.";
				return "Tak wyszlo. Nie wszystko trzeba planowac.";
			case I_PARTY:
				return g.s.inParty ? "Bo razem sie lepiej expi." : (g.voice == V_GRINDER ? "Bo sam szybciej expie." : "Bo nikt mnie nie zaprosil :)");
			case I_GUILD:
				return g.s.inGuild ? "Bo sa tam fajni ludzie." : "Jakos nie trafilem na odpowiednia.";
			case I_REST: case I_HP:
				return "Bo mnie moby porzadnie obily.";
			case I_GOAL: case I_NEXT_PLAN:
			{
				static const char* const kWhy[V_COUNT] = {
					"Bo chce byc mocniejszy.", "Bo chce zobaczyc, co jest dalej.", "Bo to sie oplaci.",
					"Bo lubie wyzwania.", "Bo wtedy moge wiecej pomoc ekipie." };
				return kWhy[g.voice];
			}
			case I_GENERAL:
			{
				const TTopicPack* pack = FindTopicPack(g.m.lastTopic);
				if (pack)
					return PickField(g, pack->why, 2);
				break;
			}
			default:
				break;
		}
		static const char* const k[] = { "Tak wyszlo.", "Po prostu tak.", "Dobre pytanie. Tak jakos." };
		return PBC_SAY(g, k);
	}

	inline std::string GenFollowUp(TGen& g, const TAnalysis& a)
	{
		switch (a.follow)
		{
			case F_WHY:
				return GenWhy(g, a.subject);
			case F_CONFIRM:
			{
				static const char* const k[] = { "Serio.", "No mowie ci.", "Naprawde.", "Tak, na serio." };
				std::string out = PBC_SAY(g, k);
				if (IsGameIntent(a.subject) && g.rng.Chance(50))
				{
					TAnalysis sub = a;
					sub.intent = a.subject;
					sub.follow = F_NONE;
					std::string more = GenerateOne(g, sub);
					if (!more.empty() && more != g.m.lastReply)
						Append(out, more);
				}
				return out;
			}
			case F_HOW:
				switch (a.subject)
				{
					case I_PARTY_REQUEST: return "Po prostu zapros mnie do grupy.";
					case I_ACTIVITY: case I_ACTIVITY_LOCATION: return "Normalnie, bije i zbieram drop.";
					case I_GOLD: return g.voice == V_MERCHANT ? "Handel. Kupic tanio, sprzedac drozej." : "Drop i troche handlu.";
					case I_LEVEL: return "Expem, jak kazdy.";
					case I_HOW_ARE_YOU: return GenHowAreYou(g);
					default: return "Normalnie, po swojemu.";
				}
			case F_WHEN:
				if (a.subject == I_NEXT_PLAN || a.subject == I_TRAVEL || a.subject == I_REST || a.subject == I_GOAL)
				{
					static const char* const k[] = { "Pewnie za chwile.", "Niedlugo, zobaczymy." };
					return PBC_SAY(g, k);
				}
				return "Nie wiem dokladnie, zobaczymy.";
			case F_NEXT:
			{
				const TTopicPack* pack = FindTopicPack(g.m.lastTopic);
				if (pack && g.askBack.empty())
				{
					const std::string q = PickField(g, pack->ask, 2);
					if (!q.empty())
					{
						g.askBackKind = ASK_TOPIC;
						g.askBackTopic = (ETopic)pack->topic;
						return "No i tyle :) " + q;
					}
				}
				return "No i tyle :)";
			}
			case F_COUNT:
				return "Ale czego?";
			case F_THIS:
				return "Ale ktory?";
			case F_WHO:
				return "Kto? O kim mowisz?";
			case F_WHAT:
			default:
			{
				static const char* const k[] = { "Co masz na mysli?", "Ale co dokladnie?", "Hm?" };
				return PBC_SAY(g, k);
			}
		}
	}

	inline std::string GenAnswerToBot(TGen& g, const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		const bool yes = c.Has(C_YES) || c.Has(C_ACK) || c.Has(C_POSITIVE) || c.Has(C_HAPPY);
		const bool no = c.Has(C_NO) || c.Has(C_NEGATIVE) || c.Has(C_SAD);
		// The question the line answers travels with it: the memory has
		// already closed it by the time the reply is composed.
		switch (a.answeredAsk != ASK_NONE ? (int)a.answeredAsk : (int)g.m.botAsk)
		{
			case ASK_HOW_ARE_YOU:
				if (no)
				{
					static const char* const k[] = { "Oj, szkoda. Bedzie lepiej.", "Kiepsko... Trzymaj sie." };
					return PBC_SAY(g, k);
				}
				if (yes || c.Has(C_POSITIVE))
				{
					static const char* const k[] = { "To dobrze!", "Super.", "No i git." };
					return PBC_SAY(g, k);
				}
				return "No, to jak u mnie.";
			case ASK_ACTIVITY:
				if (c.Has(C_EXP) || c.Has(C_HIT) || c.Has(C_MOB))
					return "O, to powodzenia na expie!";
				if (c.Has(C_FISH))
					return "Lowienie to relaks. Powodzenia.";
				if (c.Has(C_TRADE) || c.Has(C_SHOP))
					return "Handel to dobra rzecz. Obys dobrze sprzedal.";
				if (a.tokens.Has("nic"))
					return "Tez czasem tak mam.";
				{
					static const char* const k[] = { "Aha, rozumiem.", "No to spoko.", "Tez fajnie." };
					return PBC_SAY(g, k);
				}
			case ASK_JOIN:
				if (yes)
					return "To zapros mnie do PT!";
				if (no)
					return "Szkoda, moze innym razem.";
				return "To jak, zapraszasz?";
			case ASK_FOUND:
				if (yes || c.Has(C_POSITIVE))
					return "O, gratki!";
				return "Nastepnym razem sie uda.";
			case ASK_SUMMON:
				// "Po co mam przyjsc?" - a reason is what was asked for.
				if (SummonHasReason(a))
					return SummonGo(g);
				if (no)
					return "No to zostaje przy swoim.";
				return "Hm, to jednak zostane przy swoim.";
			case ASK_TOPIC:
			default:
			{
				const TTopicPack* pack = FindTopicPack(g.m.botAskTopic);
				if (yes && !no)
				{
					static const char* const k[] = { "No to mamy cos wspolnego.", "O, fajnie.", "Tez tak mam." };
					return PBC_SAY(g, k);
				}
				if (no && !yes)
				{
					static const char* const k[] = { "A widzisz, kazdy ma inaczej.", "Aha, rozumiem." };
					return PBC_SAY(g, k);
				}
				if (pack)
					return PickField(g, pack->react, 4);
				return "Ciekawe.";
			}
		}
	}

	inline std::string GenReaction(TGen& g, const TAnalysis& a)
	{
		// A line that needs nothing back gets nothing back, often.
		switch (a.intent)
		{
			case I_LAUGH:
			{
				if (g.rng.Chance(g.Bad() ? 70 : 40))
					return std::string();
				static const char* const k[] = { "Hehe", "xD", "Haha", ":D" };
				return PBC_SAY(g, k);
			}
			case I_ACK:
			{
				if (g.rng.Chance(g.Bad() ? 75 : 55))
					return std::string();
				static const char* const k[] = { "No.", "Mhm.", "No wlasnie.", "Dokladnie." };
				return PBC_SAY(g, k);
			}
			case I_YES:
			{
				if (g.rng.Chance(50))
					return std::string();
				static const char* const k[] = { "No dobra.", "Ok.", "Tez tak mysle." };
				return PBC_SAY(g, k);
			}
			default:
			{
				if (g.rng.Chance(50))
					return std::string();
				static const char* const k[] = { "Aha.", "No dobra.", "Ok, rozumiem." };
				return PBC_SAY(g, k);
			}
		}
	}

	inline std::string GenUnknownQuestion(TGen& g, const TAnalysis& a)
	{
		(void)a;
		static const char* const kSteer[] = {
			"Hmm, ciezko powiedziec.", "Dobre pytanie. Sam nie wiem.", "Nie wiem, nigdy sie nad tym nie zastanawialem.",
			"Nie mam pojecia, szczerze." };
		std::string out = PBC_SAY(g, kSteer);
		if (g.rng.Chance(25) && g.askBack.empty())
		{
			g.askBack = "A czemu pytasz?";
			g.askBackKind = ASK_NONE;
		}
		return out;
	}

	inline std::string GenUnknownStatement(TGen& g, const TAnalysis& a)
	{
		const TConceptSet& c = a.concepts;
		if (c.Has(C_POSITIVE))
		{
			static const char* const k[] = { "O, fajnie!", "Gratki!", "No to super." };
			return PBC_SAY(g, k);
		}
		if (c.Has(C_NEGATIVE))
		{
			static const char* const k[] = { "Oj, szkoda.", "Bywa... Nastepnym razem bedzie lepiej.", "Kiepsko." };
			return PBC_SAY(g, k);
		}
		if (a.tokens.Has("tez") && a.tokens.Has("ja"))
			return "No to tak jak ja.";
		if (c.Has(C_EXP) && c.Has(C_ME))
			return "O, to powodzenia na expie.";
		static const char* const k[] = { "Aha, rozumiem.", "No tak.", "Ciekawe.", "Mhm, jasne." };
		std::string out = PBC_SAY(g, k);
		if (!g.Bad() && g.askBack.empty() && g.rng.Chance(g.voice == V_SOCIAL ? 40 : 20))
		{
			static const char* const kq[] = { "A co u ciebie?", "A ty co teraz robisz?" };
			g.askBack = PBC_SAY(g, kq);
			g.askBackKind = g.askBack[2] == 'c' ? ASK_HOW_ARE_YOU : ASK_ACTIVITY;
		}
		return out;
	}

	// -------------------------------------------------------------- dispatch

	inline std::string GenerateOne(TGen& g, const TAnalysis& a)
	{
		const TAnalysis* saved = g.a;
		g.a = &a;
		std::string out;
		switch (a.intent)
		{
			case I_GREETING: out = GenGreeting(g, false); break;
			case I_FAREWELL:
				out = GenFarewell(g);
				if (ReleaseSummonFor(g))
					Append(out, "Wracam do swoich spraw.");
				break;
			case I_THANKS:
			{
				if (ReleaseSummonFor(g))
				{
					static const char* const k[] = {
						"Nie ma sprawy! To wracam do swoich spraw.", "Spoko, to ja lece do swoich spraw." };
					out = PBC_SAY(g, k);
					break;
				}
				static const char* const k[] = { "Nie ma sprawy.", "Spoko.", "Nie ma za co.", "Luz." };
				out = PBC_SAY(g, k);
				break;
			}
			case I_APOLOGY:
				out = g.m.negative > 0 ? "No dobra, zapomnijmy." : "Spoko, nic sie nie stalo.";
				break;
			case I_HOW_ARE_YOU: out = GenHowAreYou(g); break;
			case I_HELP: out = GenHelp(g); break;
			case I_IS_BOT: out = GenIsBot(g); break;
			case I_INSULT:
				out = GenInsult(g);
				if (ReleaseSummonFor(g))
					Append(out, "Radz sobie sam.");
				break;
			case I_PRAISE: out = GenPraise(g); break;
			case I_AGE: out = GenAge(g); break;
			case I_ORIGIN: out = GenOrigin(g); break;
			case I_KS: out = GenKs(g); break;
			case I_READY: out = GenReady(g); break;
			case I_GOODLUCK: out = GenGoodLuck(g); break;
			case I_BRB: out = GenBrb(g); break;
			case I_PRICE: out = GenPrice(g); break;
			case I_ITEMSHOP: out = GenItemShop(g); break;
			case I_NAME: out = GenName(g); break;
			case I_LEVEL: out = GenLevel(g); break;
			case I_CLASS: out = GenClass(g); break;
			case I_EMPIRE: out = GenEmpire(g); break;
			case I_PERSONALITY: out = GenPersonality(g); break;
			case I_MOOD: out = GenMood(g); break;
			case I_ACTIVITY: out = GenActivity(g); break;
			case I_ACTIVITY_LOCATION: out = GenActivityLocation(g); break;
			case I_LOCATION: out = GenLocation(g); break;
			case I_TARGET: out = GenTarget(g); break;
			case I_MOB_COUNT: out = GenMobCount(g); break;
			case I_GOAL: out = GenGoal(g); break;
			case I_NEXT_PLAN: out = GenNextPlan(g); break;
			case I_HP: out = GenHp(g); break;
			case I_GOLD: out = GenGold(g); break;
			case I_HORSE: out = GenHorse(g); break;
			case I_EQUIPMENT: out = GenEquipment(g); break;
			case I_INVENTORY: out = GenInventory(g); break;
			case I_INVENTORY_SPACE: out = GenInventorySpace(g); break;
			case I_ITEM_OWN: out = GenItemOwn(g); break;
			case I_PARTY: out = GenParty(g); break;
			case I_GUILD: out = GenGuild(g); break;
			case I_FISHING: out = GenFishing(g); break;
			case I_MINING: out = GenMining(g); break;
			case I_HERBALISM: out = GenHerbalism(g); break;
			case I_BIOLOGIST: out = GenBiologist(g); break;
			case I_METIN: out = GenMetin(g); break;
			case I_DEMON_TOWER: out = GenDemonTower(g); break;
			case I_GUILD_WAR: out = GenGuildWar(g); break;
			case I_MERCENARY: out = GenMercenary(g); break;
			case I_PARTY_REQUEST: out = GenPartyRequest(g); break;
			case I_SHOP: out = GenShop(g); break;
			case I_MARKET: out = GenMarket(g); break;
			case I_BUY: out = GenBuy(g); break;
			case I_SELL: out = GenSell(g); break;
			case I_SKILLS: out = GenSkills(g); break;
			case I_PVP: out = GenPvp(g); break;
			case I_TRAVEL: out = GenTravel(g); break;
			case I_REST: out = GenRest(g); break;
			case I_REFINE: out = GenRefine(g); break;
			case I_MISSIONS: out = GenMissions(g); break;
			case I_DEATH: out = GenDeath(g); break;
			case I_RELATIONSHIP: out = GenRelationship(g); break;
			case I_TIME_HERE: out = GenTimeHere(g); break;
			case I_MAP_OPINION: out = GenMapOpinion(g); break;
			case I_DROP_LUCK: out = GenDropLuck(g); break;
			case I_PROGRESS_TODAY: out = GenProgressToday(g); break;
			case I_BUILD: out = GenBuild(g); break;
			case I_BUFFS: out = GenBuffs(g); break;
			case I_SUMMON: out = GenSummon(g); break;
			case I_DISMISS: out = GenDismiss(g); break;
			case I_FOLLOW_UP: out = GenFollowUp(g, a); break;
			case I_ANSWER_TO_BOT: out = GenAnswerToBot(g, a); break;
			case I_ACK: case I_LAUGH: case I_YES: case I_NO: out = GenReaction(g, a); break;
			case I_GENERAL: out = GenGeneral(g); break;
			case I_UNKNOWN_QUESTION: out = GenUnknownQuestion(g, a); break;
			default: out = GenUnknownStatement(g, a); break;
		}
		g.a = saved;
		return out;
	}
}

#endif
