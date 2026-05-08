#include "expansion.h"

#include "core/profiler.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/technology.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPANSION_REGION_SCAN_PER_STEP 24

enum {
    EXPANSION_WORK_TARGET,
    EXPANSION_WORK_OVERSEAS,
    EXPANSION_WORK_DONE
};

static char expansion_reasons[MAX_CIVS][96];
static int next_expansion_month[MAX_CIVS];

static int region_expansion_score(int civ_id, int region_id, int resource_pressure);

static int simulation_month_index(void) {
    return year * 12 + month - 1;
}

static int expansion_stage(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    return clamp(civs[civ_id].tech_stage, 0, 10);
}

void expansion_reset(void) {
    memset(next_expansion_month, 0, sizeof(next_expansion_month));
    memset(expansion_reasons, 0, sizeof(expansion_reasons));
}

static int region_claimable_by_alive_civ(const NaturalRegion *region) {
    int owner;

    if (!region || !region->alive) return 0;
    owner = region->owner_civ;
    return owner < 0 || owner >= civ_count || !civs[owner].alive;
}

int expansion_resource_score_for_civ(int civ_id) {
    CountrySummary summary;

    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return 0;
    summary = summarize_country(civ_id);
    return (summary.food + summary.livestock + summary.wood + summary.stone + summary.minerals +
            summary.water + summary.pop_capacity + summary.money + summary.habitability) / 2;
}

int expansion_need_for_civ(int civ_id, int resource_score) {
    PopulationSummary population = population_country_summary(civ_id);
    int scarcity = clamp(22 - resource_score, 0, 22) * 4;
    return max(population.pressure, scarcity);
}

int expansion_threshold_for_civ(int civ_id) {
    int threshold;
    if (civ_id < 0 || civ_id >= civ_count) return 100;
    threshold = 106 - civs[civ_id].expansion * 4 - civs[civ_id].logistics / 2;
    threshold = threshold * 100 / technology_expansion_percent(civ_id);
    return clamp(threshold, 58, 112);
}

static int expansion_cooldown_months(int civ_id, ExpansionAIDiagnostics ai) {
    int stage = expansion_stage(civ_id);
    int min_wait = stage >= 3 ? 6 : (stage >= 2 ? 10 : 18);
    int max_wait = stage >= 3 ? 12 : (stage >= 2 ? 18 : 30);
    int trait = civs[civ_id].expansion + civs[civ_id].logistics / 2 + civs[civ_id].governance / 3;
    int pressure = clamp(ai.expansion_desire - ai.expansion_threshold, 0, 60) / 8;
    int wait = max_wait - trait / 2 - pressure;

    if (ai.land_adjacent_unowned_regions > 4 || ai.shallow_sea_reachable_regions > 2) wait -= 2;
    if (ai.population_pressure > 100 || ai.resource_pressure > 70) wait -= 2;
    return clamp(wait, min_wait, max_wait);
}

static int expansion_retry_months(int civ_id) {
    int stage = expansion_stage(civ_id);
    return stage >= 3 ? 3 : (stage >= 2 ? 4 : 6);
}

static int region_has_near_owner_path(int region_id, int civ_id) {
    const NaturalRegion *region = regions_get(region_id);
    int i;

    if (!region_claimable_by_alive_civ(region)) return 0;
    for (i = 0; i < region->neighbor_count; i++) {
        int neighbor = region->neighbors[i];
        const NaturalRegion *near_region;
        int j;

        if (neighbor < 0 || neighbor >= region_count) continue;
        near_region = regions_get(neighbor);
        if (!near_region || !near_region->alive) continue;
        if (near_region->owner_civ == civ_id) return 1;
        if (!region_claimable_by_alive_civ(near_region)) continue;
        for (j = 0; j < near_region->neighbor_count; j++) {
            int next = near_region->neighbors[j];
            if (next >= 0 && next < region_count && natural_regions[next].owner_civ == civ_id) return 1;
        }
    }
    return 0;
}

static int owned_region_count_for_civ(int civ_id) {
    int i;
    int count = 0;

    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].owner_civ == civ_id) count++;
    }
    return count;
}

static void finalize_expansion_ai(int civ_id, ExpansionAIDiagnostics *ai) {
    ai->adjacent_unowned_regions = ai->land_adjacent_unowned_regions;
    ai->nearby_unowned_regions = ai->land_adjacent_unowned_regions + ai->land_nearby_unowned_regions;
    ai->port_unowned_regions = ai->port_candidate_regions;
    ai->expansion_desire = ai->expansion_need + clamp(ai->land_adjacent_unowned_regions * 12 +
                           ai->land_nearby_unowned_regions * 5 +
                           ai->shallow_sea_reachable_regions * 8 +
                           ai->maritime_reachable_regions * 5 +
                           ai->deep_sea_reachable_regions * 3, 0, 90) +
                           civs[civ_id].expansion * 3 + civs[civ_id].logistics * 2;
    ai->expansion_desire = ai->expansion_desire * ai->tech_expansion_percent / 100;
    ai->claim_cooldown_months = expansion_cooldown_months(civ_id, *ai);
    ai->months_until_next_claim = max(0, next_expansion_month[civ_id] - simulation_month_index());
    ai->claim_budget = ai->months_until_next_claim > 0 ? 0 : 1;
}

static ExpansionAIDiagnostics expansion_land_diagnostics(int civ_id, int resource_score) {
    ExpansionAIDiagnostics ai;
    int i;
    int viable_regions = 0;

    memset(&ai, 0, sizeof(ai));
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return ai;
    ai.population_pressure = population_pressure_for_civ(civ_id);
    ai.resource_pressure = clamp(22 - resource_score, 0, 22) * 4;
    ai.expansion_need = max(ai.population_pressure, ai.resource_pressure);
    ai.expansion_threshold = expansion_threshold_for_civ(civ_id);
    ai.tech_expansion_percent = technology_expansion_percent(civ_id);
    for (i = 0; i < region_count; i++) {
        const NaturalRegion *region = regions_get(i);
        if (region) viable_regions++;
        if (!region_claimable_by_alive_civ(region)) continue;
        ai.global_unowned_regions++;
        if (regions_region_has_owner_neighbor(i, civ_id)) ai.land_adjacent_unowned_regions++;
        else if (region_has_near_owner_path(i, civ_id)) ai.land_nearby_unowned_regions++;
    }
    ai.global_unowned_percent = viable_regions > 0 ? ai.global_unowned_regions * 100 / viable_regions : 0;
    finalize_expansion_ai(civ_id, &ai);
    return ai;
}

ExpansionAIDiagnostics expansion_ai_diagnostics(int civ_id, int resource_score) {
    ExpansionAIDiagnostics ai = expansion_land_diagnostics(civ_id, resource_score);
    MaritimeExpansionDiagnostics sea;

    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return ai;
    maritime_expansion_diagnostics(civ_id, resource_score, &sea);
    ai.port_candidate_regions = sea.port_candidate_regions;
    ai.shallow_sea_reachable_regions = sea.shallow_reachable_regions;
    ai.maritime_reachable_regions = sea.maritime_reachable_regions;
    ai.deep_sea_reachable_regions = sea.deep_reachable_regions;
    finalize_expansion_ai(civ_id, &ai);
    return ai;
}

const char *expansion_last_reason(int civ_id) {
    if (civ_id < 0 || civ_id >= MAX_CIVS || !expansion_reasons[civ_id][0]) return "No expansion decision yet.";
    return expansion_reasons[civ_id];
}

int expansion_civ_months_until_claim(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return 9999;
    return max(0, next_expansion_month[civ_id] - simulation_month_index());
}

static int expansion_attempts_for_civ(int civ_id, int resource_score, ExpansionAIDiagnostics ai) {
    int frontier_chance;
    int sea_targets = ai.shallow_sea_reachable_regions + ai.maritime_reachable_regions +
                      ai.deep_sea_reachable_regions;

    if (ai.expansion_desire < ai.expansion_threshold) {
        if (ai.land_adjacent_unowned_regions <= 0 && sea_targets <= 0) return 0;
        frontier_chance = clamp(14 + civs[civ_id].expansion * 2 + civs[civ_id].logistics +
                                ai.land_adjacent_unowned_regions * 2 + sea_targets +
                                (ai.tech_expansion_percent - 100) / 2, 10, 70);
        if (rnd(100) >= frontier_chance) return 0;
        return 1;
    }
    (void)resource_score;
    return 1;
}

static void reset_work_best(ExpansionWorkState *work) {
    work->sample_index = 0;
    work->target_found = 0;
    work->best_x = -1;
    work->best_y = -1;
    work->best_score = -1000000;
    work->target_score = 0;
}

static int region_growth_pressure(const NaturalRegion *region, CountrySummary country,
                                  int resource_pressure) {
    int score = resource_pressure * 5;
    int food_need = clamp(6 - country.food, 0, 6);
    int water_need = clamp(6 - country.water, 0, 6);
    int wood_need = clamp(5 - country.wood, 0, 5);
    int stone_need = clamp(5 - country.stone, 0, 5);
    int ore_need = clamp(5 - country.minerals, 0, 5);

    score += region->average_stats.food * food_need * 4;
    score += region->average_stats.water * water_need * 4;
    score += region->average_stats.wood * wood_need * 2;
    score += region->average_stats.stone * stone_need * 2;
    score += region->average_stats.minerals * ore_need * 3;
    score += region->average_stats.money * 3 + region->resource_diversity * 10;
    return score;
}

static int region_expansion_score(int civ_id, int region_id, int resource_pressure) {
    const NaturalRegion *region = regions_get(region_id);
    CountrySummary country = summarize_country(civ_id);
    int score;

    if (!region_claimable_by_alive_civ(region)) return -1000000;
    if (!regions_region_has_owner_neighbor(region_id, civ_id)) return -1000000;
    if (region->capital_x < 0 || region->tile_count < 8) return -1000000;

    score = region->development_score + region->cradle_score / 2;
    score += region_growth_pressure(region, country, resource_pressure);
    score += region->habitability * 9 + region->viable_direction_count * 12;
    score += region->tile_count / 3 + (region->has_port_site ? 20 : 0);
    score += civs[civ_id].logistics * 5 + civs[civ_id].governance * 3 + civs[civ_id].expansion * 4;
    score -= region->movement_difficulty * 8;
    score -= region->natural_defense * 2;
    if (world_nearby_enemy_border(civ_id, region->center_x, region->center_y, 3)) score -= 80;
    score += rnd(24);
    return score;
}

static void scan_region_targets(ExpansionWorkState *work, int scan_limit) {
    DWORD scan_start = GetTickCount();
    int start = work->sample_index;
    int end = min(work->sample_index + scan_limit, region_count);

    profiler_add_scanned_regions(end - start);
    while (work->sample_index < end) {
        int region_id = work->sample_index++;
        int score = region_expansion_score(work->civ_id, region_id, work->resource_pressure);
        if (score > work->best_score) {
            work->best_score = score;
            work->best_x = region_id;
            work->best_y = 0;
            work->target_found = score > -1000000;
        }
    }
    profiler_add_expansion_target_search_ms((int)(GetTickCount() - scan_start));
}

static int resolve_region_expansion(ExpansionWorkState *work, char *log, size_t log_size) {
    Civilization *civ = &civs[work->civ_id];
    int drive;
    int threshold;
    int tech = technology_expansion_percent(work->civ_id);

    if (!work->target_found || work->best_x < 0) {
        snprintf(expansion_reasons[work->civ_id], sizeof(expansion_reasons[work->civ_id]),
                 "No adjacent unowned natural region found; land blocked or unreachable.");
        next_expansion_month[work->civ_id] = simulation_month_index() + expansion_retry_months(work->civ_id);
        return 0;
    }
    drive = civ->logistics * 6 + civ->governance * 3 + civ->cohesion +
            civ->adaptation * 2 + civ->expansion * 4 + work->resource_pressure * 3 +
            work->best_score / 8 + civ->population / 12000 + rnd(38);
    drive = drive * tech / 100;
    threshold = 104 + clamp(work->resource_score - 24, 0, 18) -
                clamp(civ->logistics + civ->governance + civ->expansion, 0, 30);
    threshold = clamp(threshold, 74, 118);
    if (drive <= threshold) {
        snprintf(expansion_reasons[work->civ_id], sizeof(expansion_reasons[work->civ_id]),
                 "Frontier found, but population/resource/admin drive %d <= %d.", drive, threshold);
        next_expansion_month[work->civ_id] = simulation_month_index() + expansion_retry_months(work->civ_id);
        return 0;
    }
    profiler_add_expansion_claim_attempt(1);
    if (!regions_claim_for_civ(work->best_x, work->civ_id, -1, 1)) {
        snprintf(expansion_reasons[work->civ_id], sizeof(expansion_reasons[work->civ_id]),
                 city_count >= MAX_CITIES ?
                 "Region claim failed: city cap or no admin city available." :
                 "Region claim failed: ownership or admin-city constraints blocked it.");
        next_expansion_month[work->civ_id] = simulation_month_index() + expansion_retry_months(work->civ_id);
        return 0;
    }
    profiler_add_claimed_regions(1);
    profiler_add_expansion_claim_success(1);
    next_expansion_month[work->civ_id] =
        simulation_month_index() + expansion_cooldown_months(work->civ_id,
                                                            expansion_land_diagnostics(work->civ_id,
                                                                                       work->resource_score));
    snprintf(expansion_reasons[work->civ_id], sizeof(expansion_reasons[work->civ_id]),
             "Claimed adjacent region %d; cooldown %d months; tech expansion x%d%%.",
             work->best_x, max(0, next_expansion_month[work->civ_id] - simulation_month_index()), tech);
    append_log(log, log_size, "[Expansion] %s claimed a neighboring region. ",
               civilization_display_name_for_language(work->civ_id, 0));
    return 1;
}

void expansion_work_begin(ExpansionWorkState *work, int civ_id, int resource_score) {
    ExpansionAIDiagnostics ai;
    int now = simulation_month_index();

    if (!work) return;
    work->active = civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
    work->civ_id = civ_id;
    work->resource_score = resource_score;
    ai = expansion_land_diagnostics(civ_id, resource_score);
    work->resource_pressure = ai.expansion_need;
    work->attempts_remaining = work->active ? expansion_attempts_for_civ(civ_id, resource_score, ai) : 0;
    work->stage = EXPANSION_WORK_TARGET;
    reset_work_best(work);
    if (!work->active) {
        work->stage = EXPANSION_WORK_DONE;
        return;
    }
    if (now < next_expansion_month[civ_id]) {
        snprintf(expansion_reasons[civ_id], sizeof(expansion_reasons[civ_id]),
                 "Expansion cooldown: next claim window in %d months.",
                 next_expansion_month[civ_id] - now);
        work->active = 0;
        work->stage = EXPANSION_WORK_DONE;
        return;
    }
    if (work->attempts_remaining <= 0) {
        snprintf(expansion_reasons[civ_id], sizeof(expansion_reasons[civ_id]),
                 "Claim skipped: desire %d vs threshold %d; land %d, shallow %d, maritime %d.",
                 ai.expansion_desire, ai.expansion_threshold, ai.land_adjacent_unowned_regions,
                 ai.shallow_sea_reachable_regions, ai.maritime_reachable_regions);
        next_expansion_month[civ_id] = now + expansion_retry_months(civ_id);
        work->stage = EXPANSION_WORK_OVERSEAS;
    }
}

int expansion_work_step(ExpansionWorkState *work, char *log, size_t log_size) {
    if (!work || !work->active) return 0;
    if (!civs[work->civ_id].alive) {
        work->stage = EXPANSION_WORK_DONE;
        work->active = 0;
        return 0;
    }
    if (work->stage == EXPANSION_WORK_TARGET) {
        scan_region_targets(work, EXPANSION_REGION_SCAN_PER_STEP);
        if (work->sample_index < region_count) return 1;
        resolve_region_expansion(work, log, log_size);
        work->attempts_remaining--;
        work->stage = EXPANSION_WORK_OVERSEAS;
        reset_work_best(work);
        return 1;
    }
    if (work->stage == EXPANSION_WORK_OVERSEAS) {
        ExpansionAIDiagnostics ai = expansion_land_diagnostics(work->civ_id, work->resource_score);
        int reachable_sea = 0;
        if (ai.land_adjacent_unowned_regions <= 0 ||
            ai.expansion_need >= ai.expansion_threshold + 22) {
            int before = owned_region_count_for_civ(work->civ_id);
            ai = expansion_ai_diagnostics(work->civ_id, work->resource_score);
            reachable_sea = ai.shallow_sea_reachable_regions + ai.maritime_reachable_regions +
                            ai.deep_sea_reachable_regions;
            profiler_add_expansion_claim_attempt(1);
            maritime_try_overseas_expansion(work->civ_id, work->resource_score, log, log_size);
            if (owned_region_count_for_civ(work->civ_id) > before) {
                profiler_add_claimed_regions(1);
                profiler_add_expansion_claim_success(1);
                next_expansion_month[work->civ_id] =
                    simulation_month_index() + expansion_cooldown_months(work->civ_id, ai);
                snprintf(expansion_reasons[work->civ_id], sizeof(expansion_reasons[work->civ_id]),
                         "Claimed sea-reachable region; shallow %d, maritime %d, deep %d; cooldown %d months.",
                         ai.shallow_sea_reachable_regions, ai.maritime_reachable_regions,
                         ai.deep_sea_reachable_regions,
                         max(0, next_expansion_month[work->civ_id] - simulation_month_index()));
            } else if (reachable_sea > 0) {
                snprintf(expansion_reasons[work->civ_id], sizeof(expansion_reasons[work->civ_id]),
                         "Sea reachable S%d/R%d/D%d; skipped by chance, score, or city cap.",
                         ai.shallow_sea_reachable_regions, ai.maritime_reachable_regions,
                         ai.deep_sea_reachable_regions);
            } else {
                snprintf(expansion_reasons[work->civ_id], sizeof(expansion_reasons[work->civ_id]),
                         "Land: no adjacent target. Shallow sea: 0 reachable. Maritime: 0 reachable. Port candidates: %d.",
                         ai.port_candidate_regions);
            }
        } else if (work->attempts_remaining <= 0 &&
                   strncmp(expansion_reasons[work->civ_id], "Desire", 6) == 0) {
            snprintf(expansion_reasons[work->civ_id], sizeof(expansion_reasons[work->civ_id]),
                     "Nearby land remains; overseas expansion suppressed.");
        }
        work->stage = EXPANSION_WORK_DONE;
        work->active = 0;
        return 1;
    }
    work->active = 0;
    return 0;
}

int expansion_work_done(const ExpansionWorkState *work) {
    return !work || !work->active || work->stage >= EXPANSION_WORK_DONE;
}

void expansion_update_civilization(int civ_id, int resource_score, char *log, size_t log_size) {
    ExpansionWorkState work;

    expansion_work_begin(&work, civ_id, resource_score);
    while (!expansion_work_done(&work)) expansion_work_step(&work, log, log_size);
}
