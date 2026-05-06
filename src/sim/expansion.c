#include "expansion.h"

#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/regions.h"
#include "sim/simulation.h"

#include <stdlib.h>

#define EXPANSION_REGION_SCAN_PER_STEP 80

enum {
    EXPANSION_WORK_TARGET,
    EXPANSION_WORK_OVERSEAS,
    EXPANSION_WORK_DONE
};

int expansion_need_for_civ(int civ_id, int resource_score) {
    PopulationSummary population = population_country_summary(civ_id);
    int scarcity = clamp(22 - resource_score, 0, 22) * 4;
    return max(population.pressure, scarcity);
}

int expansion_threshold_for_civ(int civ_id) {
    if (civ_id < 0 || civ_id >= civ_count) return 100;
    return clamp(106 - civs[civ_id].expansion * 4 - civs[civ_id].logistics / 2, 66, 112);
}

static int expansion_attempts_for_civ(int civ_id, int resource_score) {
    int expansion_need = expansion_need_for_civ(civ_id, resource_score);
    int threshold = expansion_threshold_for_civ(civ_id);
    int attempts;

    if (expansion_need < threshold) return 0;
    attempts = 1 + (expansion_need - threshold) / 18 + civs[civ_id].expansion / 7;
    if (resource_score > 26 && attempts > 1 && rnd(100) < 45) attempts--;
    return attempts;
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

    if (!region || !region->alive || region->owner_civ >= 0) return -1000000;
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
    int end = min(work->sample_index + scan_limit, region_count);

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
}

static int resolve_region_expansion(ExpansionWorkState *work, char *log, size_t log_size) {
    Civilization *civ = &civs[work->civ_id];
    int drive;
    int threshold;

    if (!work->target_found || work->best_x < 0) return 0;
    drive = civ->logistics * 6 + civ->governance * 3 + civ->cohesion +
            civ->adaptation * 2 + civ->expansion * 4 + work->resource_pressure * 3 +
            work->best_score / 8 + civ->population / 12000 + rnd(38);
    threshold = 128 + clamp(work->resource_score - 22, 0, 18);
    if (drive <= threshold) return 0;
    if (!regions_claim_for_civ(work->best_x, work->civ_id, -1, 1)) return 0;
    append_log(log, log_size, "%s claimed a neighboring region. ", civ->name);
    return 1;
}

void expansion_work_begin(ExpansionWorkState *work, int civ_id, int resource_score) {
    if (!work) return;
    work->active = civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
    work->civ_id = civ_id;
    work->resource_score = resource_score;
    work->resource_pressure = expansion_need_for_civ(civ_id, resource_score);
    work->attempts_remaining = work->active ? expansion_attempts_for_civ(civ_id, resource_score) : 0;
    work->stage = EXPANSION_WORK_TARGET;
    reset_work_best(work);
    if (work->attempts_remaining <= 0) {
        work->stage = EXPANSION_WORK_DONE;
        work->active = 0;
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
        if (work->attempts_remaining > 0) reset_work_best(work);
        else {
            work->stage = EXPANSION_WORK_OVERSEAS;
            reset_work_best(work);
        }
        return 1;
    }
    if (work->stage == EXPANSION_WORK_OVERSEAS) {
        maritime_try_overseas_expansion(work->civ_id, work->resource_score, log, log_size);
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
