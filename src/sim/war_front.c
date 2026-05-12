#include "sim/war_front.h"

#include "core/game_state.h"
#include "sim/diplomacy.h"
#include "sim/sea_lanes.h"
#include "sim/vassal.h"

static int valid_alive_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

static int group_contains(const int *ids, int count, int civ_id) {
    int i;
    for (i = 0; i < count; i++) if (ids[i] == civ_id) return 1;
    return 0;
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

static int land_contact_between_groups(const int *a_ids, int a_count,
                                       const int *b_ids, int b_count,
                                       int civ_a, int civ_b) {
    int i;
    int j;
    int flags = 0;
    for (i = 0; i < a_count; i++) {
        for (j = 0; j < b_count; j++) {
            DiplomacyRelation rel = diplomacy_relation(a_ids[i], b_ids[j]);
            if (rel.border_length <= 0) continue;
            if (a_ids[i] == civ_a && b_ids[j] == civ_b) flags |= WAR_FRONT_LAND;
            else flags |= WAR_FRONT_VASSAL_LAND;
        }
    }
    return flags;
}

static int sea_contact_between_groups(const int *a_ids, int a_count,
                                      const int *b_ids, int b_count,
                                      int civ_a, int civ_b) {
    int ca;
    int cb;
    int flags = 0;
    for (ca = 0; ca < city_count; ca++) {
        if (!cities[ca].alive || !cities[ca].port || !group_contains(a_ids, a_count, cities[ca].owner)) continue;
        for (cb = 0; cb < city_count; cb++) {
            if (!cities[cb].alive || !cities[cb].port || !group_contains(b_ids, b_count, cities[cb].owner)) continue;
            if (!sea_lanes_network_connected(ca, cb)) continue;
            if (cities[ca].owner == civ_a && cities[cb].owner == civ_b) flags |= WAR_FRONT_SEA;
            else flags |= WAR_FRONT_VASSAL_SEA;
        }
    }
    return flags;
}

int war_front_flags(int civ_a, int civ_b) {
    int a_ids[MAX_CIVS];
    int b_ids[MAX_CIVS];
    int a_count;
    int b_count;
    int flags;
    if (!valid_alive_civ(civ_a) || !valid_alive_civ(civ_b) || civ_a == civ_b) return WAR_FRONT_NONE;
    a_count = war_front_control_group(civ_a, a_ids, MAX_CIVS);
    b_count = war_front_control_group(civ_b, b_ids, MAX_CIVS);
    if (a_count <= 0 || b_count <= 0) return WAR_FRONT_NONE;
    flags = land_contact_between_groups(a_ids, a_count, b_ids, b_count, civ_a, civ_b);
    flags |= sea_contact_between_groups(a_ids, a_count, b_ids, b_count, civ_a, civ_b);
    return flags;
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
