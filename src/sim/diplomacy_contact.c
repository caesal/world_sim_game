#include "sim/diplomacy.h"

#include "sim/sea_lanes.h"

static int valid_alive_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

DiplomacyContactKind diplomacy_direct_contact_kind(int civ_a, int civ_b) {
    int sea_kind;
    if (!valid_alive_civ(civ_a) || !valid_alive_civ(civ_b) || civ_a == civ_b) return DIP_CONTACT_NONE;
    if (diplomacy_land_contact_stats(civ_a, civ_b, NULL, NULL)) return DIP_CONTACT_LAND_BORDER;
    sea_kind = sea_lanes_contact_kind(civ_a, civ_b);
    if (sea_kind == 1) return DIP_CONTACT_SHALLOW_SEA_NETWORK;
    if (sea_kind == 2) return DIP_CONTACT_DEEP_SEA_NETWORK;
    return DIP_CONTACT_NONE;
}

DiplomacyContactKind diplomacy_current_contact_kind(int civ_a, int civ_b) {
    return diplomacy_direct_contact_kind(civ_a, civ_b);
}
