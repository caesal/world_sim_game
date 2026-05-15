#include "sim/diplomacy.h"

#include "sim/sea_lanes.h"
#include "sim/vassal.h"

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

static int collect_control_contacts(int civ_id, int *out, int max_ids) {
    int count = 0;
    int i;
    int overlord;
    if (!out || max_ids <= 0 || !valid_alive_civ(civ_id)) return 0;
    out[count++] = civ_id;
    overlord = vassal_overlord(civ_id);
    if (overlord >= 0 && valid_alive_civ(overlord) && count < max_ids) {
        out[count++] = overlord;
        return count;
    }
    for (i = 0; i < civ_count && count < max_ids; i++) {
        if (i != civ_id && vassal_is_direct(civ_id, i) && valid_alive_civ(i)) out[count++] = i;
    }
    return count;
}

DiplomacyContactKind diplomacy_current_contact_kind(int civ_a, int civ_b) {
    int a_ids[MAX_CIVS];
    int b_ids[MAX_CIVS];
    int a_count;
    int b_count;
    int i;
    int j;
    DiplomacyContactKind direct = diplomacy_direct_contact_kind(civ_a, civ_b);
    if (direct != DIP_CONTACT_NONE) return direct;
    if (!valid_alive_civ(civ_a) || !valid_alive_civ(civ_b) || civ_a == civ_b) return DIP_CONTACT_NONE;

    a_count = collect_control_contacts(civ_a, a_ids, MAX_CIVS);
    b_count = collect_control_contacts(civ_b, b_ids, MAX_CIVS);
    for (i = 0; i < a_count; i++) {
        for (j = 0; j < b_count; j++) {
            if (a_ids[i] == civ_a && b_ids[j] == civ_b) continue;
            if (a_ids[i] == b_ids[j]) continue;
            if (diplomacy_direct_contact_kind(a_ids[i], b_ids[j]) != DIP_CONTACT_NONE) {
                return DIP_CONTACT_VASSAL_PROXY;
            }
        }
    }
    return DIP_CONTACT_NONE;
}
