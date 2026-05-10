#include "sim/diplomacy.h"

const char *diplomacy_status_name(DiplomacyStatus status) {
    switch (status) {
        case DIPLOMACY_NONE: return "None";
        case DIPLOMACY_PEACE: return "Peace";
        case DIPLOMACY_ALLIANCE: return "Alliance";
        case DIPLOMACY_TENSE: return "Tense";
        case DIPLOMACY_TRUCE: return "Truce";
        case DIPLOMACY_WAR: return "War";
        case DIPLOMACY_VASSAL: return "Vassal";
        default: return "Unknown";
    }
}
