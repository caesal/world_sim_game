#include "collapse.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/civ_colors.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/collapse_partition.h"
#include "sim/economy.h"
#include "sim/fragmentation_diag.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/regions_settlement.h"
#include "sim/simulation.h"
#include "sim/territory_integrity.h"
#include "sim/vassal.h"

#include <stdio.h>
#include <string.h>

static char collapse_reasons[MAX_CIVS][EVENT_LOG_LEN];
static char collapse_block_details[MAX_CIVS][EVENT_LOG_LEN];
static int immediate_attempt_month[MAX_CIVS];

#define COLLAPSE_GRACE_YEARS 35
#define COLLAPSE_GRACE_MONTHS (COLLAPSE_GRACE_YEARS * 12)

static int collapse_month_index(void) {
    return year * 12 + month;
}

int collapse_decade_chance_for_disorder(int disorder) {
    if (disorder >= 100) return 0;
    if (disorder >= 95) return 65;
    if (disorder >= 90) return 45;
    if (disorder >= 85) return 30;
    if (disorder >= 80) return 20;
    if (disorder >= 75) return 10;
    return 0;
}

static void collapse_refresh_world(void) {
    territory_integrity_repair_capitals();
    world_recalculate_territory();
    population_sync_all();
    ports_refresh_city_regions();
    maritime_mark_routes_dirty();
    diplomacy_mark_contacts_dirty();
    dirty_mark_territory();
    world_visual_revision++;
}

static int capital_region_for_civ(int civ_id) {
    int city_id = civs[civ_id].capital_city;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return -1;
    return regions_region_for_city(city_id);
}

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

CollapseBlockReason collapse_block_reason(int civ_id) {
    int cap_region;
    int owned_regions = 0;

    if (!world_generated || civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) {
        return COLLAPSE_BLOCK_NOT_ALIVE;
    }
    owned_regions = owned_region_count_for_civ(civ_id, NULL);
    if (owned_regions == 1 && economy_effective_disorder_for_civ(civ_id) >= 100) return COLLAPSE_BLOCK_NONE;
    if (civilization_slot_capacity_left() <= 0) return COLLAPSE_BLOCK_MAX_CIVS;
    cap_region = capital_region_for_civ(civ_id);
    if (cap_region < 0) return COLLAPSE_BLOCK_NO_CAPITAL_REGION;
    if (owned_regions > 1) {
        int i;
        for (i = 0; i < region_count; i++) {
            if (!natural_regions[i].alive || natural_regions[i].owner_civ != civ_id) continue;
            if (i != cap_region) return COLLAPSE_BLOCK_NONE;
        }
    }
    if (owned_regions <= 1) return COLLAPSE_BLOCK_ONLY_CORE_LEFT;
    return COLLAPSE_BLOCK_NO_SPLITTABLE_REGION;
}

const char *collapse_block_reason_text(CollapseBlockReason reason) {
    switch (reason) {
        case COLLAPSE_BLOCK_NONE: return "Ready.";
        case COLLAPSE_BLOCK_NOT_ALIVE: return "Country is invalid or has fallen.";
        case COLLAPSE_BLOCK_MAX_CIVS: return "Country limit reached.";
        case COLLAPSE_BLOCK_NO_CAPITAL_REGION: return "No valid capital region.";
        case COLLAPSE_BLOCK_NO_SPLITTABLE_REGION: return "No splittable non-capital region.";
        case COLLAPSE_BLOCK_ONLY_CORE_LEFT: return "Only capital/core region remains.";
        case COLLAPSE_BLOCK_CITY_CAP: return "City limit reached.";
        default: return "Unknown collapse blocker.";
    }
}

static const char *collapse_format_block_reason(int civ_id, CollapseBlockReason reason) {
    if (civ_id < 0 || civ_id >= MAX_CIVS) return collapse_block_reason_text(reason);
    if (reason == COLLAPSE_BLOCK_MAX_CIVS) {
        snprintf(collapse_block_details[civ_id], sizeof(collapse_block_details[civ_id]),
                 "Cannot collapse: no reusable or free country slots. Slots used %d/%d, alive %d, fallen reusable %d.",
                 civ_count, MAX_CIVS, civilization_alive_count(), civilization_reusable_slot_count());
        return collapse_block_details[civ_id];
    }
    snprintf(collapse_block_details[civ_id], sizeof(collapse_block_details[civ_id]),
             "%s", collapse_block_reason_text(reason));
    return collapse_block_details[civ_id];
}

static int create_successor_civ(int parent, int index, int seed_region) {
    Civilization parent_state = civs[parent];
    Civilization *child;
    int child_id;

    (void)index;
    child_id = civilization_allocate_slot(1);
    if (child_id < 0) return -1;
    child = &civs[child_id];
    civilization_assign_generated_name_for_heritage(child, parent_state.heritage,
                                                    civilization_pick_unused_name_id_for_heritage(parent_state.heritage));
    child->symbol = (char)('a' + (child_id % 26));
    child->color = civilization_pick_distinct_color(child_id, 0, parent, seed_region);
    child->alive = 1;
    child->population = 0;
    child->territory = 0;
    child->aggression = parent_state.aggression;
    child->expansion = parent_state.expansion;
    child->defense = parent_state.defense;
    child->culture = parent_state.culture;
    child->governance = parent_state.governance;
    child->cohesion = clamp(parent_state.cohesion + 1 + rnd(3), 0, 10);
    child->production = parent_state.production;
    child->military = parent_state.military;
    child->commerce = parent_state.commerce;
    child->logistics = parent_state.logistics;
    child->innovation = parent_state.innovation;
    child->adaptation = parent_state.adaptation;
    child->tech_stage = clamp(parent_state.tech_stage, 0, 10);
    child->tech_progress = parent_state.tech_progress;
    child->deep_sea_route_unlocked_event_done = parent_state.deep_sea_route_unlocked_event_done;
    child->disorder = 0;
    child->disorder_carry_x10 = 0;
    child->disorder_resource = 0;
    child->disorder_plague = clamp(parent_state.disorder_plague / 2, 0, 30);
    child->disorder_migration = clamp(parent_state.disorder_migration / 2, 0, 30);
    child->disorder_stability = clamp(max(25, parent_state.disorder_stability / 2), 0, 30);
    child->plague_recovery_months = parent_state.plague_recovery_months;
    child->war_recovery_months = parent_state.war_recovery_months;
    child->collapse_grace_months = COLLAPSE_GRACE_MONTHS;
    child->capital_city = -1;
    economy_initialize_civ(child_id);
    if (seed_region >= 0 && seed_region < region_count) {
        NaturalRegion *region = &natural_regions[seed_region];
        int city_id = regions_activate_local_city(seed_region, child_id,
                                                  max(800, region->average_stats.pop_capacity * 600), 1, 1);
        if (city_id >= 0) {
            cities[city_id].capital = 1;
            dirty_mark_city();
            child->capital_city = city_id;
        }
    }
    return child_id;
}

static void apply_post_collapse_grace(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    civs[civ_id].disorder = 0;
    civs[civ_id].disorder_carry_x10 = 0;
    civs[civ_id].collapse_grace_months = COLLAPSE_GRACE_MONTHS;
    world_invalidate_country_summary_cache();
    dirty_mark_civ_stats();
}

static int claim_region_direct(int region_id, int owner) {
    return regions_claim_for_civ(region_id, owner, natural_regions[region_id].city_id, 1);
}

static void collapse_release_vassal_relations(int civ_id) {
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

static int collapse_civ(int civ_id, CollapseCause cause) {
    int cap_region;
    int owned_regions;
    int successor_limit;
    CollapsePartitionResult partition;
    int formed = 0;
    int i;
    int former_overlord = -1;
    int parent_asset_total;
    int parent_treasury_snapshot;
    CollapseBlockReason block;

    fragmentation_diag_record_collapse_attempt();
    if (civ_id < 0 || civ_id >= MAX_CIVS) {
        event_log_push_structured(EVENT_TYPE_DEBUG_NOTICE, EVENT_SEVERITY_WARNING,
                                  -1, -1, -1, -1, 0, 0, "Collapse blocked: invalid country.");
        fragmentation_diag_record_collapse_failure();
        return 0;
    }
    if (cause == COLLAPSE_CAUSE_PRESSURE && civs[civ_id].collapse_grace_months > 0) {
        snprintf(collapse_reasons[civ_id], sizeof(collapse_reasons[civ_id]),
                 "Collapse grace: %d years %d months left; pressure collapse skipped.",
                 civs[civ_id].collapse_grace_months / 12, civs[civ_id].collapse_grace_months % 12);
        fragmentation_diag_record_collapse_failure();
        return 0;
    }
    cap_region = capital_region_for_civ(civ_id);
    block = collapse_block_reason(civ_id);
    if (block != COLLAPSE_BLOCK_NONE) {
        snprintf(collapse_reasons[civ_id], sizeof(collapse_reasons[civ_id]),
                 "Collapse blocked: %s", collapse_format_block_reason(civ_id, block));
        event_log_push_structured(EVENT_TYPE_COLLAPSE_FAILED, EVENT_SEVERITY_WARNING,
                                  civ_id, -1, -1, -1, 0, 0, collapse_reasons[civ_id]);
        fragmentation_diag_record_collapse_failure();
        return 0;
    }
    if (collapse_single_province_preview(civ_id, NULL) != COLLAPSE_SINGLE_NONE) {
        int collapsed = collapse_single_province_execute(civ_id, cause);
        if (collapsed) fragmentation_diag_record_collapse_success(0);
        else fragmentation_diag_record_collapse_failure();
        return collapsed;
    }
    former_overlord = vassal_overlord(civ_id);
    collapse_release_vassal_relations(civ_id);
    owned_regions = owned_region_count_for_civ(civ_id, NULL);
    parent_asset_total = economy_owned_region_asset_total(civ_id);
    parent_treasury_snapshot = max(0, civs[civ_id].treasury);
    successor_limit = collapse_successor_count_for_owned_regions(owned_regions);
    successor_limit = min(successor_limit, civilization_slot_capacity_left());
    successor_limit = min(successor_limit, owned_regions - 1);
    if (collapse_partition_build(civ_id, cap_region, successor_limit, &partition) <= 0) {
        successor_limit = 0;
    }
    for (i = 0; i < partition.successor_count && formed < successor_limit; i++) {
        int child;
        int claimed = 0;
        int child_asset = 0;
        int r;
        child = create_successor_civ(civ_id, formed, partition.successor_capital_region[i]);
        if (child < 0) break;
        for (r = 0; r < partition.successor_region_count[i]; r++) {
            int region_id = partition.successor_regions[i][r];
            if (natural_regions[region_id].owner_civ != civ_id) continue;
            if (!claim_region_direct(region_id, child)) continue;
            claimed++;
            child_asset += economy_region_asset(region_id);
        }
        if (claimed <= 0) {
            civilization_reset_slot_state(child);
            continue;
        }
        diplomacy_start_truce(civ_id, child, 45, 20);
        economy_split_treasury_snapshot_to_child(civ_id, child, parent_treasury_snapshot,
                                                 child_asset, parent_asset_total);
        formed++;
    }
    if (formed > 0) {
        apply_post_collapse_grace(civ_id);
        snprintf(collapse_reasons[civ_id], sizeof(collapse_reasons[civ_id]),
                 "Collapse formed %d successor state%s.", formed, formed == 1 ? "" : "s");
        event_log_push_structured(EVENT_TYPE_COLLAPSE_SUCCEEDED, EVENT_SEVERITY_DANGER,
                                  civ_id, -1, -1, -1, formed, 0, "");
        if (former_overlord >= 0) {
            event_log_push_structured(EVENT_TYPE_VASSAL_SELF_COLLAPSE_RELEASED, EVENT_SEVERITY_INFO,
                                      civ_id, former_overlord, -1, -1, formed, 0, "");
        }
        civilization_colors_debug_check();
        collapse_refresh_world();
        fragmentation_diag_record_collapse_success(formed);
    } else {
        snprintf(collapse_reasons[civ_id], sizeof(collapse_reasons[civ_id]),
                 "Collapse failed: no splittable non-capital region.");
        event_log_push_structured(EVENT_TYPE_COLLAPSE_FAILED, EVENT_SEVERITY_WARNING,
                                  civ_id, -1, -1, -1, 0, 0, collapse_reasons[civ_id]);
        fragmentation_diag_record_collapse_failure();
    }
    return formed > 0;
}

int collapse_probability_for_disorder(int disorder) {
    return collapse_decade_chance_for_disorder(disorder);
}

int collapse_can_trigger(int civ_id) {
    return collapse_block_reason(civ_id) == COLLAPSE_BLOCK_NONE;
}

const char *collapse_trigger_block_reason(int civ_id) {
    CollapseBlockReason reason = collapse_block_reason(civ_id);
    return collapse_format_block_reason(civ_id, reason);
}

const char *collapse_last_reason(int civ_id) {
    if (civ_id < 0 || civ_id >= MAX_CIVS || !collapse_reasons[civ_id][0]) {
        return "No collapse check yet.";
    }
    return collapse_reasons[civ_id];
}

int collapse_trigger(int civ_id, CollapseCause cause) {
    return collapse_civ(civ_id, cause);
}

int collapse_check_immediate(int civ_id, CollapseCause cause) {
    int collapsed;
    int now = collapse_month_index();

    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive ||
        economy_effective_disorder_for_civ(civ_id) < 100) return 0;
    if (cause != COLLAPSE_CAUSE_CIVIL_UNREST && civs[civ_id].collapse_grace_months > 0) {
        snprintf(collapse_reasons[civ_id], sizeof(collapse_reasons[civ_id]),
                 "Collapse grace: %d years %d months left; immediate collapse skipped.",
                 civs[civ_id].collapse_grace_months / 12, civs[civ_id].collapse_grace_months % 12);
        return 0;
    }
    if (immediate_attempt_month[civ_id] == now + 1 && cause != COLLAPSE_CAUSE_CIVIL_UNREST) return 0;
    immediate_attempt_month[civ_id] = now + 1;
    collapsed = collapse_civ(civ_id, cause);
    return collapsed;
}

int collapse_grace_months_left(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return 0;
    return max(0, civs[civ_id].collapse_grace_months);
}

void collapse_update_immediate(void) {
    int i;

    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive && economy_effective_disorder_for_civ(i) >= 100) {
            collapse_check_immediate(i, COLLAPSE_CAUSE_PRESSURE);
        }
    }
}

void collapse_update_decade(void) {
    int i;

    for (i = 0; i < civ_count; i++) {
        int chance;
        int roll;
        int effective_disorder;
        if (!civs[i].alive) continue;
        effective_disorder = economy_effective_disorder_for_civ(i);
        if (effective_disorder >= 100) {
            snprintf(collapse_reasons[i], sizeof(collapse_reasons[i]),
                     "Immediate collapse path owns disorder 100 checks.");
            continue;
        }
        if (civs[i].collapse_grace_months > 0) {
            snprintf(collapse_reasons[i], sizeof(collapse_reasons[i]),
                     "Collapse grace: %d years %d months left; ordinary 25-year roll skipped.",
                     civs[i].collapse_grace_months / 12, civs[i].collapse_grace_months % 12);
            continue;
        }
        chance = collapse_decade_chance_for_disorder(effective_disorder);
        if (chance <= 0) {
            snprintf(collapse_reasons[i], sizeof(collapse_reasons[i]),
                     "No collapse risk: effective disorder %d below 75.", effective_disorder);
            continue;
        }
        roll = rnd(100);
        if (roll < chance) {
            snprintf(collapse_reasons[i], sizeof(collapse_reasons[i]),
                     "25-year check triggered: effective disorder %d, chance %d%%, rolled %d, needed below %d.",
                     effective_disorder, chance, roll, chance);
            collapse_civ(i, COLLAPSE_CAUSE_PRESSURE);
        } else {
            snprintf(collapse_reasons[i], sizeof(collapse_reasons[i]),
                     "25-year check failed: effective disorder %d, chance %d%%, rolled %d, needed below %d.",
                     effective_disorder, chance, roll, chance);
            event_log_push_structured(EVENT_TYPE_COLLAPSE_FAILED, EVENT_SEVERITY_WARNING,
                                      i, -1, -1, -1, chance, roll, collapse_reasons[i]);
        }
    }
}
