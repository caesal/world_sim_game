#include "sim/war_front.h"

#include "core/game_state.h"
#include "sim/diplomacy.h"
#include "sim/vassal.h"

static int valid_alive_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

int war_front_control_group(int civ_id, int *out_ids, int max_ids) {
    int count = 0;
    int i;
    if (!out_ids || max_ids <= 0 || !valid_alive_civ(civ_id)) return 0;
    out_ids[count++] = civ_id;
    if (vassal_overlord(civ_id) >= 0) return count;
    for (i = 0; i < civ_count && count < max_ids; i++) {
        if (i != civ_id && vassal_is_direct(civ_id, i) && valid_alive_civ(i)) {
            out_ids[count++] = i;
        }
    }
    return count;
}

static int contact_between_groups(const int *a_ids, int a_count,
                                  const int *b_ids, int b_count,
                                  int civ_a, int civ_b) {
    int i;
    int j;
    int flags = 0;
    for (i = 0; i < a_count; i++) {
        for (j = 0; j < b_count; j++) {
            DiplomacyContactKind kind;
            int direct = a_ids[i] == civ_a && b_ids[j] == civ_b;
            kind = diplomacy_direct_contact_kind(a_ids[i], b_ids[j]);
            if (kind == DIP_CONTACT_LAND_BORDER) flags |= direct ? WAR_FRONT_LAND : WAR_FRONT_VASSAL_LAND;
            else if (kind == DIP_CONTACT_SHALLOW_SEA_NETWORK || kind == DIP_CONTACT_DEEP_SEA_NETWORK) {
                flags |= direct ? WAR_FRONT_SEA : WAR_FRONT_VASSAL_SEA;
            }
        }
    }
    return flags;
}

int war_front_flags(int civ_a, int civ_b) {
    int a_ids[MAX_CIVS];
    int b_ids[MAX_CIVS];
    int a_count;
    int b_count;
    if (!valid_alive_civ(civ_a) || !valid_alive_civ(civ_b) || civ_a == civ_b) return WAR_FRONT_NONE;
    a_count = war_front_control_group(civ_a, a_ids, MAX_CIVS);
    b_count = war_front_control_group(civ_b, b_ids, MAX_CIVS);
    if (a_count <= 0 || b_count <= 0) return WAR_FRONT_NONE;
    return contact_between_groups(a_ids, a_count, b_ids, b_count, civ_a, civ_b);
}

int war_has_active_front(int civ_a, int civ_b) {
    return war_front_flags(civ_a, civ_b) != WAR_FRONT_NONE;
}

const char *war_front_reason_en(int flags) {
    if (flags & WAR_FRONT_LAND) return "Land";
    if (flags & WAR_FRONT_SEA) return "Sea lane";
    if (flags & WAR_FRONT_VASSAL_LAND) return "Vassal land";
    if (flags & WAR_FRONT_VASSAL_SEA) return "Vassal sea";
    return "None";
}

const char *war_front_reason_zh(int flags) {
    if (flags & WAR_FRONT_LAND) return "陆地";
    if (flags & WAR_FRONT_SEA) return "航道";
    if (flags & WAR_FRONT_VASSAL_LAND) return "附庸陆地";
    if (flags & WAR_FRONT_VASSAL_SEA) return "附庸航道";
    return "无";
}
