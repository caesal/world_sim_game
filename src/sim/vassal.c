#include "sim/vassal.h"

#include "core/game_state.h"
#include "sim/diplomacy.h"
#include "sim/simulation.h"
#include "sim/war.h"

#include <stdio.h>

static int valid_alive_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

int vassal_overlord(int civ_id) {
    int i;

    if (!valid_alive_civ(civ_id)) return -1;
    for (i = 0; i < civ_count; i++) {
        DiplomacyRelation relation;
        if (i == civ_id || !valid_alive_civ(i)) continue;
        relation = diplomacy_relation(i, civ_id);
        if (relation.state == DIPLOMACY_VASSAL &&
            relation.overlord == i && relation.vassal == civ_id) {
            return i;
        }
    }
    return -1;
}

int vassal_root_overlord(int civ_id) {
    int seen[MAX_CIVS] = {0};
    int current = civ_id;
    int next;

    while (valid_alive_civ(current) && !seen[current]) {
        seen[current] = 1;
        next = vassal_overlord(current);
        if (next < 0) return current == civ_id ? -1 : current;
        current = next;
    }
    return -1;
}

int vassal_is_direct(int overlord, int vassal) {
    DiplomacyRelation relation;

    if (!valid_alive_civ(overlord) || !valid_alive_civ(vassal) || overlord == vassal) return 0;
    relation = diplomacy_relation(overlord, vassal);
    return relation.state == DIPLOMACY_VASSAL &&
           relation.overlord == overlord && relation.vassal == vassal;
}

int vassal_collect_direct(int overlord, int *out_ids, int max_ids) {
    int i;
    int count = 0;

    if (!valid_alive_civ(overlord)) return 0;
    for (i = 0; i < civ_count; i++) {
        if (i == overlord || !valid_alive_civ(i)) continue;
        if (!vassal_is_direct(overlord, i)) continue;
        if (out_ids && count < max_ids) out_ids[count] = i;
        count++;
    }
    return count;
}

int vassal_direct_count(int overlord) {
    return vassal_collect_direct(overlord, NULL, 0);
}

int vassal_governance_disorder(int overlord) {
    int n = vassal_direct_count(overlord);
    return clamp(n * 10, 0, 100);
}

int vassal_callable_soldiers(int vassal) {
    return valid_alive_civ(vassal) ? war_current_soldiers_for_civ(vassal) * 70 / 100 : 0;
}

int vassal_total_callable_soldiers(int overlord) {
    int ids[MAX_CIVS];
    int count = vassal_collect_direct(overlord, ids, MAX_CIVS);
    int total = 0;
    int i;

    for (i = 0; i < count; i++) total += vassal_callable_soldiers(ids[i]);
    return total;
}

int vassal_estimated_resource_tribute_from(int vassal) {
    CountrySummary summary;
    int resources;

    if (!valid_alive_civ(vassal)) return 0;
    summary = summarize_country(vassal);
    resources = summary.food + summary.livestock + summary.wood +
                summary.stone + summary.minerals + summary.water;
    return resources * 2 / 3;
}

int vassal_estimated_resource_tribute_total(int overlord) {
    int ids[MAX_CIVS];
    int count = vassal_collect_direct(overlord, ids, MAX_CIVS);
    int total = 0;
    int i;

    for (i = 0; i < count; i++) total += vassal_estimated_resource_tribute_from(ids[i]);
    return total;
}

void vassal_release(int vassal) {
    int overlord = vassal_overlord(vassal);
    if (overlord < 0) return;
    diplomacy_start_truce(overlord, vassal, 0, 50);
}

void vassal_release_all(int overlord) {
    int ids[MAX_CIVS];
    int count = vassal_collect_direct(overlord, ids, MAX_CIVS);
    int i;

    for (i = 0; i < count; i++) vassal_release(ids[i]);
}

static void clear_direct_wars_for_vassal(int vassal) {
    int i;

    war_end_direct_for_civ(vassal);
    for (i = 0; i < civ_count; i++) {
        if (i == vassal || !valid_alive_civ(i)) continue;
        if (diplomacy_status(vassal, i) == DIPLOMACY_WAR) {
            diplomacy_start_truce(vassal, i, 0, 45);
        }
    }
}

int vassal_make(int overlord, int vassal, int relation_score) {
    int children[MAX_CIVS];
    int child_count;
    int old_overlord;
    int i;
    char text[EVENT_LOG_LEN];

    if (!valid_alive_civ(overlord) || !valid_alive_civ(vassal) || overlord == vassal) return 0;
    old_overlord = vassal_root_overlord(overlord);
    if (old_overlord >= 0) overlord = old_overlord;
    if (overlord == vassal) return 0;
    if (vassal_root_overlord(overlord) == vassal) return 0;

    old_overlord = vassal_overlord(vassal);
    if (old_overlord >= 0) vassal_release(vassal);
    child_count = vassal_collect_direct(vassal, children, MAX_CIVS);
    for (i = 0; i < child_count; i++) {
        vassal_release(children[i]);
        diplomacy_start_vassal(overlord, children[i], relation_score);
    }
    clear_direct_wars_for_vassal(vassal);
    diplomacy_start_vassal(overlord, vassal, relation_score);
    snprintf(text, sizeof(text), "[Diplomacy] %.48s became vassal of %.48s.",
             civilization_display_name_for_language(vassal, 0),
             civilization_display_name_for_language(overlord, 0));
    event_log_push(text);
    world_invalidate_country_summary_cache();
    return 1;
}

void vassal_normalize_all(void) {
    int overlord;
    int child;

    for (child = 0; child < civ_count; child++) {
        int root;
        int other;
        if (!valid_alive_civ(child)) continue;
        overlord = vassal_overlord(child);
        if (overlord < 0) continue;
        root = vassal_root_overlord(overlord);
        if (root < 0) root = overlord;
        if (root >= 0 && root != overlord && root != child) {
            vassal_release(child);
            diplomacy_start_vassal(root, child, 50);
        }
        if (root == child || !valid_alive_civ(overlord)) vassal_release(child);
        overlord = vassal_overlord(child);
        if (overlord < 0) continue;
        for (other = 0; other < civ_count; other++) {
            DiplomacyStatus status;
            if (other == child || other == overlord || !valid_alive_civ(other)) continue;
            status = diplomacy_status(child, other);
            if (status == DIPLOMACY_WAR || status == DIPLOMACY_ALLIANCE) {
                diplomacy_start_truce(child, other, 0, 50);
            }
        }
    }
}

void vassal_update_year(void) {
    int vassal;

    vassal_normalize_all();
    for (vassal = 0; vassal < civ_count; vassal++) {
        int overlord = vassal_overlord(vassal);
        int vassal_army;
        int overlord_army;
        if (overlord < 0) continue;
        vassal_army = war_current_soldiers_for_civ(vassal);
        overlord_army = max(1, war_current_soldiers_for_civ(overlord));
        if (vassal_army * 100 > overlord_army * 90) {
            vassal_release(vassal);
            event_log_push("[Diplomacy] A strong vassal peacefully became independent.");
        } else if (vassal_army * 100 > overlord_army * 65) {
            vassal_release(vassal);
            war_start_independence(vassal, overlord);
            event_log_push("[War] A vassal declared an independence war.");
        }
    }
}
