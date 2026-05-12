#include "collapse.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/civ_colors.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
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
    dirty_mark_labels();
    world_visual_revision++;
}

static int capital_region_for_civ(int civ_id) {
    int city_id = civs[civ_id].capital_city;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return -1;
    return regions_region_for_city(city_id);
}

CollapseBlockReason collapse_block_reason(int civ_id) {
    int cap_region;
    int i;
    int owned_regions = 0;

    if (!world_generated || civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) {
        return COLLAPSE_BLOCK_NOT_ALIVE;
    }
    if (civilization_slot_capacity_left() <= 0) return COLLAPSE_BLOCK_MAX_CIVS;
    cap_region = capital_region_for_civ(civ_id);
    if (cap_region < 0) return COLLAPSE_BLOCK_NO_CAPITAL_REGION;
    for (i = 0; i < region_count; i++) {
        if (!natural_regions[i].alive || natural_regions[i].owner_civ != civ_id) continue;
        owned_regions++;
        if (i != cap_region) return COLLAPSE_BLOCK_NONE;
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
    civilization_assign_generated_name(child, civilization_pick_unused_name_id());
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
    child->cohesion = parent_state.cohesion;
    child->production = parent_state.production;
    child->military = parent_state.military;
    child->commerce = parent_state.commerce;
    child->logistics = parent_state.logistics;
    child->innovation = parent_state.innovation;
    child->adaptation = parent_state.adaptation;
    child->tech_stage = clamp(parent_state.tech_stage, 0, 10);
    child->tech_progress = parent_state.tech_progress;
    child->deep_sea_route_unlocked_event_done = parent_state.deep_sea_route_unlocked_event_done;
    child->disorder = clamp(parent_state.disorder / 2, 20, 40);
    child->disorder_resource = 0;
    child->disorder_plague = clamp(parent_state.disorder_plague / 2, 0, 30);
    child->disorder_migration = clamp(parent_state.disorder_migration / 2, 0, 30);
    child->disorder_stability = clamp(max(25, parent_state.disorder_stability / 2), 0, 30);
    child->plague_recovery_months = parent_state.plague_recovery_months;
    child->war_recovery_months = parent_state.war_recovery_months;
    child->collapse_grace_months = COLLAPSE_GRACE_MONTHS;
    child->capital_city = -1;
    if (seed_region >= 0 && seed_region < region_count) {
        NaturalRegion *region = &natural_regions[seed_region];
        int city_id = region->city_id;
        if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) {
            city_id = world_create_city(child_id, region->capital_x, region->capital_y,
                                        max(800, region->average_stats.pop_capacity * 600), 1);
        }
        if (city_id >= 0) {
            cities[city_id].owner = child_id;
            cities[city_id].capital = 1;
            child->capital_city = city_id;
        }
    }
    return child_id;
}

static void apply_post_collapse_grace(int civ_id) {
    int floor;
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return;
    floor = vassal_governance_disorder(civ_id);
    civs[civ_id].disorder_plague = clamp(civs[civ_id].disorder_plague, 0, 30);
    civs[civ_id].disorder_migration = clamp(civs[civ_id].disorder_migration, 0, 30);
    civs[civ_id].disorder_stability = clamp(civs[civ_id].disorder_stability, 0, 30);
    civs[civ_id].disorder = max(floor, clamp(civs[civ_id].disorder, 0, 40));
    civs[civ_id].collapse_grace_months = COLLAPSE_GRACE_MONTHS;
}

static void claim_region_direct(int region_id, int owner) {
    int x;
    int y;
    NaturalRegion *region = &natural_regions[region_id];
    int city_id = region->city_id;

    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) {
        city_id = world_create_city(owner, region->capital_x, region->capital_y,
                                    max(700, region->average_stats.pop_capacity * 420), 0);
        region->city_id = city_id;
    }
    if (city_id < 0) city_id = civs[owner].capital_city;
    region->owner_civ = owner;
    if (city_id >= 0 && city_id < city_count && cities[city_id].alive) cities[city_id].owner = owner;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].region_id != region_id) continue;
            world[y][x].owner = owner;
            world[y][x].province_id = city_id >= 0 ? city_id : region_id;
        }
    }
}

static int add_neighbor_regions(int parent, int child, int seed_region, int cap_region) {
    int claimed = 0;
    int frontier[MAX_REGION_NEIGHBORS + 1];
    int front_count = 1;
    int i;

    frontier[0] = seed_region;
    for (i = 0; i < front_count && claimed < 6; i++) {
        NaturalRegion *region = &natural_regions[frontier[i]];
        int n;
        if (frontier[i] == cap_region || region->owner_civ != parent) continue;
        claim_region_direct(frontier[i], child);
        claimed++;
        for (n = 0; n < region->neighbor_count && front_count < MAX_REGION_NEIGHBORS + 1; n++) {
            int neighbor = region->neighbors[n];
            if (neighbor >= 0 && neighbor < region_count && natural_regions[neighbor].owner_civ == parent) {
                frontier[front_count++] = neighbor;
            }
        }
    }
    return claimed;
}

static int collapse_civ(int civ_id, CollapseCause cause) {
    int cap_region;
    int formed = 0;
    int i;
    int released_vassals[MAX_CIVS];
    int released_count = 0;
    int former_overlord = -1;
    CollapseBlockReason block;

    if (civ_id < 0 || civ_id >= MAX_CIVS) {
        event_log_push_structured(EVENT_TYPE_DEBUG_NOTICE, EVENT_SEVERITY_WARNING,
                                  -1, -1, -1, -1, 0, 0, "Collapse blocked: invalid country.");
        return 0;
    }
    cap_region = capital_region_for_civ(civ_id);
    block = collapse_block_reason(civ_id);
    if (block != COLLAPSE_BLOCK_NONE) {
        snprintf(collapse_reasons[civ_id], sizeof(collapse_reasons[civ_id]),
                 "Collapse blocked: %s", collapse_format_block_reason(civ_id, block));
        event_log_push_structured(EVENT_TYPE_COLLAPSE_FAILED, EVENT_SEVERITY_WARNING,
                                  civ_id, -1, -1, -1, 0, 0, collapse_reasons[civ_id]);
        return 0;
    }
    former_overlord = vassal_overlord(civ_id);
    released_count = vassal_collect_direct(civ_id, released_vassals, MAX_CIVS);
    vassal_release(civ_id);
    vassal_release_all(civ_id);
    for (i = 0; i < released_count; i++) {
        event_log_push_structured(EVENT_TYPE_VASSAL_COLLAPSE_INDEPENDENCE, EVENT_SEVERITY_INFO,
                                  released_vassals[i], civ_id, -1, -1, 0, 0, "");
    }
    for (i = 0; i < region_count && formed < 4 && civilization_slot_capacity_left() > 0; i++) {
        int child;
        if (!natural_regions[i].alive || natural_regions[i].owner_civ != civ_id || i == cap_region) continue;
        if (cause == COLLAPSE_CAUSE_PRESSURE &&
            natural_regions[i].development_score < 30 && natural_regions[i].tile_count < 40) continue;
        child = create_successor_civ(civ_id, formed, i);
        if (child < 0) break;
        if (add_neighbor_regions(civ_id, child, i, cap_region) <= 0) {
            civilization_reset_slot_state(child);
            continue;
        }
        diplomacy_start_truce(civ_id, child, 45, 20);
        formed++;
    }
    if (cause == COLLAPSE_CAUSE_PRESSURE) disorder_relieve(civ_id, 25);
    else disorder_set_civil_unrest(civ_id);
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
    } else {
        snprintf(collapse_reasons[civ_id], sizeof(collapse_reasons[civ_id]),
                 "Collapse failed: no splittable non-capital region.");
        event_log_push_structured(EVENT_TYPE_COLLAPSE_FAILED, EVENT_SEVERITY_WARNING,
                                  civ_id, -1, -1, -1, 0, 0, collapse_reasons[civ_id]);
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

    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive || civs[civ_id].disorder < 100) return 0;
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
        if (civs[i].alive && civs[i].disorder >= 100) {
            collapse_check_immediate(i, COLLAPSE_CAUSE_PRESSURE);
        }
    }
}

void collapse_update_decade(void) {
    int i;

    for (i = 0; i < civ_count; i++) {
        int chance;
        int roll;
        if (!civs[i].alive) continue;
        if (civs[i].disorder >= 100) {
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
        chance = collapse_decade_chance_for_disorder(civs[i].disorder);
        if (chance <= 0) {
            snprintf(collapse_reasons[i], sizeof(collapse_reasons[i]),
                     "No collapse risk: disorder %d below 75.", civs[i].disorder);
            continue;
        }
        roll = rnd(100);
        if (roll < chance) {
            snprintf(collapse_reasons[i], sizeof(collapse_reasons[i]),
                     "25-year check triggered: disorder %d, chance %d%%, rolled %d, needed below %d.",
                     civs[i].disorder, chance, roll, chance);
            collapse_civ(i, COLLAPSE_CAUSE_PRESSURE);
        } else {
            snprintf(collapse_reasons[i], sizeof(collapse_reasons[i]),
                     "25-year check failed: disorder %d, chance %d%%, rolled %d, needed below %d.",
                     civs[i].disorder, chance, roll, chance);
            event_log_push_structured(EVENT_TYPE_COLLAPSE_FAILED, EVENT_SEVERITY_WARNING,
                                      i, -1, -1, -1, chance, roll, collapse_reasons[i]);
        }
    }
}
