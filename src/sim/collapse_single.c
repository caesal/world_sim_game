#include "sim/collapse.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/territory_integrity.h"
#include "sim/vassal.h"
#include "sim/war.h"

#include <stdio.h>

static int owned_region_count_for_civ(int civ_id, int *only_region) {
    int count = 0;
    int i;
    if (only_region) *only_region = -1;
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < region_count; i++) {
        if (!natural_regions[i].alive || natural_regions[i].owner_civ != civ_id) continue;
        if (only_region) *only_region = i;
        count++;
    }
    return count;
}

static int collapse_has_contact(int civ_id, int other) {
    return diplomacy_direct_contact_kind(civ_id, other) != DIP_CONTACT_NONE;
}

static long long collapse_strength_score(int civ_id, int war_context) {
    CountrySummary summary = summarize_country(civ_id);
    long long army = war_current_soldiers_for_civ(civ_id);
    long long population = max(0, civs[civ_id].population);
    long long resource = max(0, summary.resource_score);
    long long tech = max(0, civs[civ_id].tech_stage);
    int army_weight = war_context ? 50 : 40;
    int pop_weight = war_context ? 20 : 25;
    return army * army_weight + population * pop_weight + resource * 2000LL + tech * 15000LL;
}

static int better_collapse_candidate(int candidate, int current, int war_context) {
    long long a;
    long long b;
    if (candidate < 0 || candidate >= civ_count || !civs[candidate].alive) return current;
    if (current < 0) return candidate;
    a = collapse_strength_score(candidate, war_context);
    b = collapse_strength_score(current, war_context);
    if (a > b) return candidate;
    if (a == b && candidate < current) return candidate;
    return current;
}

CollapseSingleProvinceResult collapse_single_province_preview(int civ_id, int *out_candidate) {
    int only_region = -1;
    int overlord;
    int i;
    int best = -1;
    if (out_candidate) *out_candidate = -1;
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive || civs[civ_id].disorder < 100) {
        return COLLAPSE_SINGLE_NONE;
    }
    if (owned_region_count_for_civ(civ_id, &only_region) != 1 || only_region < 0) {
        return COLLAPSE_SINGLE_NONE;
    }
    overlord = vassal_overlord(civ_id);
    if (overlord >= 0 && collapse_has_contact(civ_id, overlord)) {
        if (out_candidate) *out_candidate = overlord;
        return COLLAPSE_SINGLE_ANNEX_OVERLORD;
    }
    for (i = 0; i < civ_count; i++) {
        if (i == civ_id || !civs[i].alive) continue;
        if (!war_active_between(civ_id, i) || !collapse_has_contact(civ_id, i)) continue;
        best = better_collapse_candidate(i, best, 1);
    }
    if (best >= 0) {
        if (out_candidate) *out_candidate = best;
        return COLLAPSE_SINGLE_ANNEX_WAR;
    }
    for (i = 0; i < civ_count; i++) {
        if (i == civ_id || !civs[i].alive || !collapse_has_contact(civ_id, i)) continue;
        best = better_collapse_candidate(i, best, 0);
    }
    if (best >= 0) {
        if (out_candidate) *out_candidate = best;
        return COLLAPSE_SINGLE_ANNEX_NEIGHBOR;
    }
    return COLLAPSE_SINGLE_UNCLAIMED;
}

static void collapse_refresh_world(void) {
    territory_integrity_repair_capitals();
    world_recalculate_territory();
    population_sync_all();
    ports_refresh_city_regions();
    maritime_mark_routes_dirty();
    diplomacy_mark_contacts_dirty();
    dirty_mark_territory();
    dirty_mark_labels();
    world_visual_revision++;
}

static void release_vassal_relations(int civ_id) {
    int released_vassals[MAX_CIVS];
    int released_count = vassal_collect_direct(civ_id, released_vassals, MAX_CIVS);
    int i;
    vassal_release(civ_id);
    vassal_release_all(civ_id);
    for (i = 0; i < released_count; i++) {
        event_log_push_structured(EVENT_TYPE_VASSAL_COLLAPSE_INDEPENDENCE, EVENT_SEVERITY_INFO,
                                  released_vassals[i], civ_id, -1, -1, 0, 0, "");
    }
}

static void transfer_region_cities(int region_id, int from, int to) {
    int i;
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner != from) continue;
        if (regions_region_for_city(i) != region_id) continue;
        cities[i].owner = to;
        cities[i].capital = 0;
    }
}

static void unclaim_single_region(int region_id, int owner) {
    int x;
    int y;
    int i;
    if (region_id < 0 || region_id >= region_count) return;
    natural_regions[region_id].owner_civ = -1;
    natural_regions[region_id].city_id = -1;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].region_id != region_id) continue;
            world[y][x].owner = -1;
            world[y][x].province_id = -1;
        }
    }
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive || cities[i].owner != owner) continue;
        if (regions_region_for_city(i) != region_id) continue;
        cities[i].alive = 0;
        cities[i].owner = -1;
        cities[i].capital = 0;
        cities[i].port = 0;
        cities[i].port_region = -1;
    }
}

static void retire_collapsed_civ(int civ_id) {
    war_end_direct_for_civ(civ_id);
    diplomacy_clear_civ(civ_id);
    civs[civ_id].alive = 0;
    civs[civ_id].capital_city = -1;
    civs[civ_id].territory = 0;
}

int collapse_single_province_execute(int civ_id, CollapseCause cause) {
    int region_id = -1;
    int candidate = -1;
    CollapseSingleProvinceResult result = collapse_single_province_preview(civ_id, &candidate);
    (void)cause;
    if (result == COLLAPSE_SINGLE_NONE ||
        owned_region_count_for_civ(civ_id, &region_id) != 1 || region_id < 0) {
        return 0;
    }
    release_vassal_relations(civ_id);
    if (candidate >= 0) {
        int city_id = natural_regions[region_id].city_id;
        if (regions_claim_for_civ(region_id, candidate, city_id, 1)) {
            transfer_region_cities(region_id, civ_id, candidate);
        } else {
            candidate = -1;
            result = COLLAPSE_SINGLE_UNCLAIMED;
        }
    }
    if (candidate < 0) unclaim_single_region(region_id, civ_id);
    retire_collapsed_civ(civ_id);
    event_log_push_structured(EVENT_TYPE_COLLAPSE_SUCCEEDED, EVENT_SEVERITY_DANGER,
                              civ_id, candidate, region_id, -1, -(int)result, 0, "");
    collapse_refresh_world();
    return 1;
}
