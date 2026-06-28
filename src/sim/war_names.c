#include "sim/war.h"

const char *war_outcome_name_impl(WarOutcome outcome) {
    switch (outcome) {
        case WAR_OUTCOME_NONE: return "None";
        case WAR_OUTCOME_ATTACKER_WIN: return "Attacker Win";
        case WAR_OUTCOME_DEFENDER_WIN: return "Defender Win";
        case WAR_OUTCOME_STALEMATE: return "Stalemate";
        default: return "Unknown";
    }
}
