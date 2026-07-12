#include "sim/vassal.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/fragmentation_diag.h"
#include "sim/regions.h"
#include "sim/regions_settlement.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "sim/world_announcement.h"

#include <stdio.h>
#include <string.h>

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

int vassal_total_soldiers(int vassal) {
    return valid_alive_civ(vassal) ? war_current_soldiers_for_civ(vassal) : 0;
}

int vassal_callable_soldiers(int vassal) {
    return vassal_total_soldiers(vassal) * 70 / 100;
}

int vassal_garrison_soldiers(int vassal) {
    return max(0, vassal_total_soldiers(vassal) - vassal_callable_soldiers(vassal));
}

int vassal_support_used_by_overlord(int overlord, int vassal) {
    return war_vassal_support_used_for_overlord(overlord, vassal);
}

int vassal_support_casualties(int vassal) {
    return war_vassal_support_casualties(vassal);
}

int vassal_total_callable_soldiers(int overlord) {
    int ids[MAX_CIVS];
    int count = vassal_collect_direct(overlord, ids, MAX_CIVS);
    int total = 0;
    int i;

    for (i = 0; i < count; i++) total += vassal_callable_soldiers(ids[i]);
    return total;
}

VassalTributeBreakdown vassal_resource_tribute_breakdown_from(int vassal) {
    VassalTributeBreakdown out;
    CountrySummary summary;

    memset(&out, 0, sizeof(out));
    if (!valid_alive_civ(vassal)) return out;
    summary = summarize_country(vassal);
    out.food = summary.food * 40 / 100;
    out.livestock = summary.livestock * 40 / 100;
    out.wood = summary.wood * 40 / 100;
    out.stone = summary.stone * 40 / 100;
    out.minerals = summary.minerals * 40 / 100;
    out.water = summary.water * 40 / 100;
    out.total = out.food + out.livestock + out.wood + out.stone + out.minerals + out.water;
    return out;
}

VassalTributeBreakdown vassal_resource_tribute_breakdown_total(int overlord) {
    int ids[MAX_CIVS];
    int count = vassal_collect_direct(overlord, ids, MAX_CIVS);
    VassalTributeBreakdown total;
    int i;

    memset(&total, 0, sizeof(total));
    for (i = 0; i < count; i++) {
        VassalTributeBreakdown item = vassal_resource_tribute_breakdown_from(ids[i]);
        total.food += item.food;
        total.livestock += item.livestock;
        total.wood += item.wood;
        total.stone += item.stone;
        total.minerals += item.minerals;
        total.water += item.water;
        total.total += item.total;
    }
    return total;
}

int vassal_estimated_resource_tribute_from(int vassal) {
    return vassal_resource_tribute_breakdown_from(vassal).total;
}

int vassal_estimated_resource_tribute_total(int overlord) {
    return vassal_resource_tribute_breakdown_total(overlord).total;
}

int vassal_annex_threshold_years(int overlord) {
    if (!valid_alive_civ(overlord)) return 130;
    return civs[overlord].tech_stage >= 10 ? 45 : 130;
}

int vassal_annex_remaining_years(int overlord, int vassal_years) {
    return max(0, vassal_annex_threshold_years(overlord) - max(0, vassal_years));
}

static void transfer_remaining_vassal_cities(int overlord, int vassal) {
    int i;
    int changed = 0;

    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner != vassal) continue;
        cities[i].owner = overlord;
        if (cities[i].capital) cities[i].capital = 0;
        changed = 1;
    }
    if (changed) dirty_mark_city();
}

static int collect_vassal_regions(int vassal, int *out_regions, int max_regions) {
    int i;
    int count = 0;

    for (i = 0; i < region_count; i++) {
        if (!natural_regions[i].alive || natural_regions[i].owner_civ != vassal) continue;
        if (count < max_regions) out_regions[count] = i;
        count++;
    }
    return count;
}

static int annex_region_city_slots_available(const int *regions, int count) {
    int i;
    int needed = 0;

    for (i = 0; i < count; i++) {
        if (regions_local_city_id(regions[i]) < 0) needed++;
    }
    return city_count + needed <= MAX_CITIES;
}

static int annex_direct_vassal(int overlord, int vassal, int vassal_years, int clear_diplomacy) {
    int owned_regions[MAX_NATURAL_REGIONS];
    int owned_count;
    int i;

    if (!vassal_is_direct(overlord, vassal)) return 0;
    owned_count = collect_vassal_regions(vassal, owned_regions, MAX_NATURAL_REGIONS);
    if (!annex_region_city_slots_available(owned_regions, owned_count)) return 0;
    for (i = 0; i < owned_count; i++) {
        int region_id = owned_regions[i];
        if (!regions_claim_for_civ(region_id, overlord, natural_regions[region_id].city_id, 1)) return 0;
    }
    transfer_remaining_vassal_cities(overlord, vassal);
    world_announcement_emit_vassal(EVENT_TYPE_VASSAL_ANNEXED,
                                   vassal, overlord, -1, vassal_years);
    war_end_direct_for_civ(vassal);
    civs[vassal].alive = 0;
    civs[vassal].capital_city = -1;
    if (clear_diplomacy) diplomacy_clear_civ(vassal);
    world_invalidate_country_summary_cache();
    return 1;
}

int vassal_try_auto_annex(DiplomacyRelation *relation) {
    int overlord;
    int vassal;

    if (!relation || relation->state != DIPLOMACY_VASSAL) return 0;
    overlord = relation->overlord;
    vassal = relation->vassal;
    if (!valid_alive_civ(overlord) || !valid_alive_civ(vassal)) return 0;
    if (relation->vassal_years < vassal_annex_threshold_years(overlord)) return 0;
    if (!annex_direct_vassal(overlord, vassal, relation->vassal_years, 0)) return 0;
    relation->state = DIPLOMACY_PEACE;
    relation->overlord = -1;
    relation->vassal = -1;
    return 1;
}

int vassal_annex_direct(int overlord, int vassal) {
    DiplomacyRelation relation;

    if (!valid_alive_civ(overlord) || !valid_alive_civ(vassal)) return 0;
    relation = diplomacy_relation(overlord, vassal);
    if (relation.state != DIPLOMACY_VASSAL ||
        relation.overlord != overlord || relation.vassal != vassal) {
        return 0;
    }
    return annex_direct_vassal(overlord, vassal, relation.vassal_years, 1);
}

void vassal_release(int vassal) {
    int overlord = vassal_overlord(vassal);
    if (overlord < 0) return;
    diplomacy_start_truce(overlord, vassal, 0, 50);
    fragmentation_diag_record_vassal_release();
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

    if (!valid_alive_civ(overlord) || !valid_alive_civ(vassal) || overlord == vassal) return 0;
    old_overlord = vassal_root_overlord(overlord);
    if (old_overlord >= 0) overlord = old_overlord;
    if (overlord == vassal) return 0;
    if (vassal_root_overlord(overlord) == vassal) return 0;

    old_overlord = vassal_overlord(vassal);
    if (old_overlord >= 0) vassal_release(vassal);
    child_count = vassal_collect_direct(vassal, children, MAX_CIVS);
    for (i = 0; i < child_count; i++) {
        int child_old_overlord = vassal_overlord(children[i]);
        vassal_release(children[i]);
        diplomacy_start_vassal(overlord, children[i], relation_score);
        world_announcement_emit_vassal(EVENT_TYPE_VASSAL_TRANSFERRED,
                                       children[i], child_old_overlord, overlord, overlord);
    }
    clear_direct_wars_for_vassal(vassal);
    diplomacy_start_vassal(overlord, vassal, relation_score);
    if (vassal_is_direct(overlord, vassal)) disorder_pacify_vassalization(vassal);
    if (old_overlord >= 0 && old_overlord != overlord) {
        world_announcement_emit_vassal(EVENT_TYPE_VASSAL_TRANSFERRED,
                                       vassal, old_overlord, overlord, overlord);
    } else {
        world_announcement_emit_vassal(EVENT_TYPE_VASSAL_CREATED,
                                       vassal, overlord, -1, 0);
    }
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
            world_announcement_emit_vassal(EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE,
                                           vassal, overlord, -1, 0);
            fragmentation_diag_record_vassal_peaceful_independence();
        } else if (vassal_army * 100 > overlord_army * 65) {
            vassal_release(vassal);
            if (war_start_independence(vassal, overlord)) {
                world_announcement_emit_vassal(EVENT_TYPE_VASSAL_INDEPENDENCE_WAR,
                                               vassal, overlord, -1, 0);
                fragmentation_diag_record_vassal_independence_war();
            } else {
                world_announcement_emit_vassal(EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE,
                                               vassal, overlord, -1, 0);
                fragmentation_diag_record_vassal_peaceful_independence();
            }
        }
    }
}
