#include "sim/enclave_resolution.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/civ_colors.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/economy.h"
#include "sim/fragmentation_diag.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/regions_settlement.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "sim/world_announcement.h"

#include <stdlib.h>
#include <string.h>

#define ENCLAVE_CHILD_GRACE_MONTHS 300

static int valid_alive_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

static int valid_component_region(int region_id) {
    return region_id >= 0 && region_id < region_count && natural_regions[region_id].alive;
}

static int neighbor_strength_close(int a, int b) {
    int high = max(abs(a), abs(b));
    int diff = abs(a - b);
    if (high <= 0) return 1;
    return diff * 100 <= high * 10;
}

static int better_land_neighbor(int candidate, int current) {
    int candidate_strength;
    int current_strength;

    if (!valid_alive_civ(candidate)) return current;
    if (current < 0) return candidate;
    candidate_strength = war_current_soldiers_for_civ(candidate);
    current_strength = war_current_soldiers_for_civ(current);
    if (!neighbor_strength_close(candidate_strength, current_strength)) {
        return candidate_strength > current_strength ? candidate : current;
    }
    if (civs[candidate].disorder != civs[current].disorder) {
        return civs[candidate].disorder < civs[current].disorder ? candidate : current;
    }
    return candidate < current ? candidate : current;
}

static int strongest_land_neighbor(int owner, const int *regions, int count) {
    unsigned char seen[MAX_CIVS];
    int best = -1;
    int i;

    memset(seen, 0, sizeof(seen));
    for (i = 0; i < count; i++) {
        int n;
        NaturalRegion *region;
        if (!valid_component_region(regions[i])) continue;
        region = &natural_regions[regions[i]];
        for (n = 0; n < region->neighbor_count; n++) {
            int neighbor = region->neighbors[n];
            int candidate;
            if (!valid_component_region(neighbor)) continue;
            candidate = natural_regions[neighbor].owner_civ;
            if (candidate == owner || candidate < 0 || candidate >= MAX_CIVS || seen[candidate]) continue;
            seen[candidate] = 1;
            best = better_land_neighbor(candidate, best);
        }
    }
    return best;
}

static int claim_component_to(int owner, const int *regions, int count) {
    int i;

    if (!valid_alive_civ(owner)) return 0;
    for (i = 0; i < count; i++) {
        int region_id = regions[i];
        int city_id;
        if (!valid_component_region(region_id)) return 0;
        city_id = natural_regions[region_id].city_id;
        if (!regions_claim_for_civ(region_id, owner, city_id, 1)) return 0;
        natural_regions[region_id].disconnected_months = 0;
        natural_regions[region_id].disconnected_component_id = -1;
    }
    return 1;
}

static void unown_component(const int *regions, int count) {
    unsigned char in_component[MAX_NATURAL_REGIONS];
    int i;
    int x;
    int y;

    memset(in_component, 0, sizeof(in_component));
    for (i = 0; i < count; i++) {
        int region_id = regions[i];
        if (!valid_component_region(region_id)) continue;
        in_component[region_id] = 1;
        natural_regions[region_id].owner_civ = -1;
        natural_regions[region_id].disconnected_months = 0;
        natural_regions[region_id].disconnected_component_id = -1;
        regions_deactivate_local_city(region_id);
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int region_id = world[y][x].region_id;
            if (region_id < 0 || region_id >= MAX_NATURAL_REGIONS || !in_component[region_id]) continue;
            world[y][x].owner = -1;
            world[y][x].province_id = -1;
        }
    }
    dirty_mark_territory();
    world_invalidate_country_summary_cache();
}

static void init_child_from_parent(int child_id, int parent_id, int seed_region,
                                   const int *regions, int region_count) {
    Civilization parent;
    Civilization *child = &civs[child_id];

    memset(&parent, 0, sizeof(parent));
    if (valid_alive_civ(parent_id)) parent = civs[parent_id];
    else {
        parent.heritage = CIV_HERITAGE_WESTERN;
        parent.aggression = parent.expansion = parent.defense = 5;
        parent.culture = parent.governance = parent.cohesion = 5;
        parent.production = parent.military = parent.commerce = 5;
        parent.logistics = parent.innovation = parent.adaptation = 5;
    }
    civilization_assign_generated_name_for_heritage(child, parent.heritage,
                                                    civilization_pick_unused_name_id_for_heritage(parent.heritage));
    child->symbol = (char)('a' + (child_id % 26));
    child->color = civilization_pick_distinct_color_for_regions(child_id, 0, parent_id,
                                                                seed_region, regions,
                                                                region_count);
    child->alive = 1;
    child->aggression = parent.aggression;
    child->expansion = parent.expansion;
    child->defense = parent.defense;
    child->culture = parent.culture;
    child->governance = parent.governance;
    child->cohesion = clamp(parent.cohesion + 1 + rnd(3), 0, 10);
    child->production = parent.production;
    child->military = parent.military;
    child->commerce = parent.commerce;
    child->logistics = parent.logistics;
    child->innovation = parent.innovation;
    child->adaptation = parent.adaptation;
    child->tech_stage = clamp(parent.tech_stage, 0, 10);
    child->tech_progress = parent.tech_progress;
    child->disorder = 35;
    child->disorder_carry_x10 = 0;
    child->disorder_plague = clamp(parent.disorder_plague / 3, 0, 15);
    child->disorder_migration = 15;
    child->disorder_stability = 12;
    child->collapse_grace_months = ENCLAVE_CHILD_GRACE_MONTHS;
    child->capital_city = -1;
    economy_initialize_civ(child_id);
}

static int create_component_country(int owner, const int *regions, int count,
                                    int months, int make_vassal, int *slot_full) {
    int child_id;
    int seed_region;
    int city_id;
    int parent_asset_total;
    int child_asset;

    if (slot_full) *slot_full = 0;
    if (count <= 0 || !regions) return -1;
    seed_region = regions[0];
    if (!valid_component_region(seed_region)) return -1;
    child_id = civilization_allocate_slot(1);
    if (child_id < 0) {
        if (slot_full) *slot_full = 1;
        return -1;
    }
    init_child_from_parent(child_id, owner, seed_region, regions, count);
    parent_asset_total = economy_owned_region_asset_total(owner);
    child_asset = economy_region_list_asset(regions, count);
    city_id = regions_activate_local_city(seed_region, child_id,
                                          max(900, natural_regions[seed_region].average_stats.pop_capacity * 450),
                                          1, 1);
    if (city_id >= 0) {
        cities[city_id].capital = 1;
        dirty_mark_city();
        civs[child_id].capital_city = city_id;
    }
    if (!claim_component_to(child_id, regions, count)) {
        fragmentation_diag_record_enclave_claim_failure();
        event_log_push_structured(EVENT_TYPE_ENCLAVE_FAILED, EVENT_SEVERITY_DANGER,
                                  owner, child_id, seed_region, -1, months, count, "claim failed");
        unown_component(regions, count);
        fragmentation_diag_record_enclave_unowned_collapse();
        civilization_reset_slot_state(child_id);
        return -1;
    }
    economy_split_treasury_to_child(owner, child_id, child_asset, parent_asset_total);
    if (make_vassal && valid_alive_civ(owner)) {
        diplomacy_start_vassal(owner, child_id, 70);
        disorder_pacify_vassalization(child_id);
        world_announcement_emit_vassal_detail(EVENT_TYPE_VASSAL_CREATED,
                                               child_id, owner, -1,
                                               seed_region, city_id, months, count);
        fragmentation_diag_record_enclave_original_vassal();
    } else {
        event_log_push_structured(EVENT_TYPE_ENCLAVE_INDEPENDENT, EVENT_SEVERITY_WARNING,
                                  child_id, owner, seed_region, city_id, months, count, NULL);
        fragmentation_diag_record_enclave_independent();
    }
    if (valid_alive_civ(owner)) disorder_add_war_pressure(owner, 8 + count / 2);
    return child_id;
}

static int resolve_join_or_unowned(int owner, const int *regions, int count, int months) {
    int target = strongest_land_neighbor(owner, regions, count);

    if (target < 0) {
        fragmentation_diag_record_enclave_fallback_no_land_neighbor();
        unown_component(regions, count);
        fragmentation_diag_record_enclave_unowned_collapse();
        return 1;
    }
    if (!claim_component_to(target, regions, count)) {
        fragmentation_diag_record_enclave_claim_failure();
        event_log_push_structured(EVENT_TYPE_ENCLAVE_FAILED, EVENT_SEVERITY_DANGER,
                                  owner, target, regions[0], -1, months, count, "claim failed");
        unown_component(regions, count);
        fragmentation_diag_record_enclave_unowned_collapse();
        return 1;
    }
    if (valid_alive_civ(owner)) disorder_add_war_pressure(owner, 8 + count / 2);
    disorder_add_migration_pressure(target, 3 + count / 3);
    event_log_push_structured(EVENT_TYPE_ENCLAVE_JOINED, EVENT_SEVERITY_WARNING,
                              owner, target, regions[0], -1, months, count, NULL);
    fragmentation_diag_record_enclave_joined_land_neighbor();
    return 1;
}

static int resolve_create_or_fallback(int owner, const int *regions, int count,
                                      int months, int make_vassal, int slot_fallback_join) {
    int slot_full = 0;
    int child;

    child = create_component_country(owner, regions, count, months, make_vassal, &slot_full);
    if (child >= 0) return 1;
    if (slot_full) {
        fragmentation_diag_record_enclave_fallback_slot_full();
        if (!slot_fallback_join) {
            unown_component(regions, count);
            fragmentation_diag_record_enclave_unowned_collapse();
            return 1;
        }
        return resolve_join_or_unowned(owner, regions, count, months);
    }
    return 1;
}

int enclave_resolve_component(int owner, const int *regions, int count, int months) {
    EnclaveResolutionEvent event;
    int tech_stage = valid_alive_civ(owner) ? civs[owner].tech_stage : 0;

    if (!regions || count <= 0) return 0;
    event = enclave_resolution_pick_event_for_roll(count, tech_stage, rnd(100));
    switch (event) {
        case ENCLAVE_RESOLUTION_INDEPENDENT:
            return resolve_create_or_fallback(owner, regions, count, months, 0, 1);
        case ENCLAVE_RESOLUTION_ORIGINAL_VASSAL:
            if (!valid_alive_civ(owner)) return resolve_create_or_fallback(owner, regions, count, months, 0, 0);
            return resolve_create_or_fallback(owner, regions, count, months, 1, 1);
        case ENCLAVE_RESOLUTION_JOIN_LAND_NEIGHBOR:
            return resolve_join_or_unowned(owner, regions, count, months);
        case ENCLAVE_RESOLUTION_UNOWNED:
        default:
            unown_component(regions, count);
            fragmentation_diag_record_enclave_unowned_collapse();
            return 1;
    }
}
