// Whether an ordinary monster is worth fighting at all.
//
// Delivered as a standalone, unit-tested module with the audit that asked for
// it (point 4: "dopuszczanie walki i ponowna ocena przeciwnika"), and kept
// exactly as delivered - no engine calls, no randomness, no stored decisions,
// so the same question gets the same answer wherever it is asked. Everything
// that touches the engine lives in BuildPlayerBotCombatContext in
// playerbot_targeting.h, which is the one place a Context is built.
//
// Included from playerbot_manager.cpp before the fragments that use it. It is
// a real header, not one of the implementation fragments: it declares nothing
// into the anonymous namespace and may be included anywhere.

#ifndef PLAYERBOT_COMBAT_VALUE_POLICY_H
#define PLAYERBOT_COMBAT_VALUE_POLICY_H

// Standalone C++98 policy. No engine calls, side effects or persisted decisions.
// Scope: ordinary monsters only. Keep existing Metin/PvP/safety rules.
namespace playerbot_combat_value {

// SERVICE_ONLY appended, never inserted: the numbering is read by nothing
// outside this header today, but the reasons are logged and compared.
//
// It is the residence rule the 8 September audit asked for - a map a bot has
// outgrown is a place to buy, to pass through and to finish a named errand in,
// and not a place to grind. COMMITTED_TRAVEL cannot express that: it refuses
// everything but defence, including the quest the bot is actually there for.
enum Mode { OBJECTIVES, COMMITTED_TRAVEL, RETREAT, SERVICE_ONLY };
enum Reason {
    REJECT_BASE_RULES, REJECT_RETREAT, REJECT_COMMITTED_TRAVEL,
    REJECT_NO_EXP_EVIDENCE, REJECT_ZERO_EXP, REJECT_LOW_EXP,
    ALLOW_SELF_DEFENSE, ALLOW_PARTY_DEFENSE, ALLOW_QUEST,
    ALLOW_MATERIAL, ALLOW_EQUIPMENT, ALLOW_EXP,
    REJECT_RESIDENCE_POLICY, REJECT_OUTGROWN_PREY, ALLOW_LURE
};

struct Policy {
    // Proposed starting heuristic, NOT measured EXP/hour or a game rule.
    int minLevelExpPercent;
    Policy() : minLevelExpPercent(20) {}
};

struct Context {
    // Must include monster type, alive, map, safety, reachability, range,
    // level/party strength and existing retry cooldowns. Default is fail closed.
    bool baseEligible;
    Mode mode;
    // Adapter verifies a real immediate threat, bounded leash/time and ability
    // to fight safely. Merely having GetVictim()!=NULL is NOT sufficient.
    bool boundedSelfDefense;
    bool boundedPartyDefense;
    // Each flag requires an ACTIVE objective, matching target and unmet need.
    // Item objectives additionally require an eligible, possible drop.
    bool activeQuestTarget;
    bool activeMaterialTarget;
    bool activeEquipmentTarget;
    // The pack a lure course is walking out to tag. A person's standing order
    // puts the bot in COMMITTED_TRAVEL so it stops grinding between courses,
    // and that refused the very monsters the course exists to fetch: the order
    // was taken, "Juz dla ciebie luruje" was said, and every course ended
    // "no_pack" a tick later while the bot stood beside the person (l0st3k,
    // 20 September). What an errand mode names is not what it may refuse.
    bool lureCourseTarget;
    // Obtain from the real server's EXP rules, not a copied level table.
    bool expEvidenceKnown;
    int levelExpPercent;
    // Adapter sets this for a village monster far enough under the bot's
    // level and far enough away that walking to it is a waste of the walk.
    // Behind the objectives: an errand is still an errand.
    bool outgrownPrey;
    // False if the engine's estimate proves the bot cannot receive EXP.
    // True requires at least positive eligible base EXP; see adapter contract.
    bool canReceiveExp;

    Context() : baseEligible(false), mode(OBJECTIVES),
        boundedSelfDefense(false), boundedPartyDefense(false),
        activeQuestTarget(false), activeMaterialTarget(false),
        activeEquipmentTarget(false), lureCourseTarget(false),
        expEvidenceKnown(false),
        levelExpPercent(0), outgrownPrey(false), canReceiveExp(false) {}
};

struct Decision {
    bool allowed;
    Reason reason;
    Decision(bool a, Reason r) : allowed(a), reason(r) {}
};

inline Decision Evaluate(const Context& c, const Policy& p = Policy()) {
    if (!c.baseEligible) return Decision(false, REJECT_BASE_RULES);
    if (c.mode == RETREAT) return Decision(false, REJECT_RETREAT);
    if (c.boundedSelfDefense) return Decision(true, ALLOW_SELF_DEFENSE);
    if (c.boundedPartyDefense) return Decision(true, ALLOW_PARTY_DEFENSE);
    // Under defence and over the errand modes: fetching the pack IS the
    // errand, and a bot saving its own life still does not stop to fetch one.
    if (c.lureCourseTarget) return Decision(true, ALLOW_LURE);
    if (c.mode == COMMITTED_TRAVEL)
        return Decision(false, REJECT_COMMITTED_TRAVEL);
    if (c.activeQuestTarget) return Decision(true, ALLOW_QUEST);
    if (c.activeMaterialTarget) return Decision(true, ALLOW_MATERIAL);
    if (c.activeEquipmentTarget) return Decision(true, ALLOW_EQUIPMENT);
    // Past the named objectives on purpose: a bot that has outgrown this map
    // may still finish an errand here, but experience is not a reason to be
    // here at all.
    if (c.mode == SERVICE_ONLY) return Decision(false, REJECT_RESIDENCE_POLICY);
    if (c.outgrownPrey) return Decision(false, REJECT_OUTGROWN_PREY);
    if (!c.expEvidenceKnown || c.levelExpPercent < 0)
        return Decision(false, REJECT_NO_EXP_EVIDENCE);
    if (!c.canReceiveExp || c.levelExpPercent == 0)
        return Decision(false, REJECT_ZERO_EXP);
    const int minimum = p.minLevelExpPercent > 0 ? p.minLevelExpPercent : 1;
    if (c.levelExpPercent < minimum) return Decision(false, REJECT_LOW_EXP);
    return Decision(true, ALLOW_EXP);
}

inline const char* ReasonName(Reason r) {
    switch (r) {
        case REJECT_BASE_RULES: return "base_rules";
        case REJECT_RETREAT: return "retreat";
        case REJECT_COMMITTED_TRAVEL: return "committed_travel";
        case REJECT_NO_EXP_EVIDENCE: return "missing_exp_evidence";
        case REJECT_ZERO_EXP: return "zero_exp";
        case REJECT_LOW_EXP: return "low_exp";
        case ALLOW_SELF_DEFENSE: return "self_defense";
        case ALLOW_PARTY_DEFENSE: return "party_defense";
        case ALLOW_QUEST: return "quest";
        case ALLOW_MATERIAL: return "material";
        case ALLOW_EQUIPMENT: return "equipment";
        case ALLOW_EXP: return "exp";
        case REJECT_RESIDENCE_POLICY: return "residence_policy";
        case REJECT_OUTGROWN_PREY: return "outgrown_prey";
        case ALLOW_LURE: return "lure_course";
    }
    return "unknown";
}
} // namespace playerbot_combat_value
#endif
