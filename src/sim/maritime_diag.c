#include "sim/maritime.h"

#include "core/profiler.h"
#include "sim/population.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/simulation.h"
#include "sim/technology.h"
#include "world/ports.h"
#include "world/terrain_query.h"

#include <stdlib.h>
#include <string.h>

#define DIAG_MAX_ROUTE_DISTANCE 700

typedef struct {
    int valid;
    int resource_score;
    int world_revision;
    int ownership_revision;
    int city_total;
    int region_total;
    int tech_stage;
    MaritimeExpansionDiagnostics data;
} MaritimeDiagCache;

static MaritimeDiagCache diag_cache[MAX_CIVS];

static int diag_cache_hit(int civ_id, int resource_score, MaritimeExpansionDiagnostics *out) {
    MaritimeDiagCache *cache;

    if (civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    cache = &diag_cache[civ_id];
    if (!cache->valid || cache->resource_score != resource_score ||
        cache->world_revision != maritime_route_revision() ||
        cache->ownership_revision != maritime_ownership_revision() ||
        cache->city_total != city_count || cache->region_total != region_count ||
        cache->tech_stage != civs[civ_id].tech_stage) return 0;
    *out = cache->data;
    return 1;
}

static void diag_cache_store(int civ_id, int resource_score, const MaritimeExpansionDiagnostics *data) {
    MaritimeDiagCache *cache;

    if (civ_id < 0 || civ_id >= MAX_CIVS || !data) return;
    cache = &diag_cache[civ_id];
    cache->valid = 1;
    cache->resource_score = resource_score;
    cache->world_revision = maritime_route_revision();
    cache->ownership_revision = maritime_ownership_revision();
    cache->city_total = city_count;
    cache->region_total = region_count;
    cache->tech_stage = civs[civ_id].tech_stage;
    cache->data = *data;
}

static int diag_civ_has_port(int civ_id) {
    int i;
    for (i = 0; i < city_count; i++) {
        if (ports_city_is_valid_port(i) && cities[i].owner == civ_id) return 1;
    }
    return 0;
}

static int diag_civ_port_in_region(int civ_id, int region, int target_x, int target_y, int *direct) {
    int i;
    int best = 1000000;

    for (i = 0; i < city_count; i++) {
        int dist;
        if (!ports_city_is_valid_port(i) || cities[i].owner != civ_id || cities[i].port_region != region) continue;
        dist = abs(cities[i].port_x - target_x) + abs(cities[i].port_y - target_y);
        if (dist < best) best = dist;
    }
    if (best == 1000000) return 0;
    if (direct) *direct = best;
    return 1;
}

static int diag_nearest_port_distance(int civ_id, int target_x, int target_y, int *direct) {
    int i;
    int best = 1000000;

    for (i = 0; i < city_count; i++) {
        int dist;
        if (!ports_city_is_valid_port(i) || cities[i].owner != civ_id) continue;
        dist = abs(cities[i].port_x - target_x) + abs(cities[i].port_y - target_y);
        if (dist < best) best = dist;
    }
    if (best == 1000000) return 0;
    if (direct) *direct = best;
    return 1;
}

static int diag_target_score(int civ_id, const NaturalRegion *region, int pressure) {
    int score = region->development_score + region->cradle_score / 2;

    score += region->average_stats.water * 5 + region->average_stats.food * 4;
    score += region->average_stats.pop_capacity * 4 + region->average_stats.money * 4;
    score += region->average_stats.wood * 2 + region->average_stats.stone * 2;
    score += region->average_stats.minerals * 3 + pressure * 8;
    score += region->resource_diversity * 10 + region->habitability * 7;
    score += civs[civ_id].logistics * 4 + civs[civ_id].commerce * 4 + civs[civ_id].expansion * 3;
    score -= region->movement_difficulty * 8;
    if (world_nearby_enemy_border(civ_id, region->center_x, region->center_y, 3)) score -= 110;
    return score;
}

static int diag_region_claimable(const NaturalRegion *region) {
    int owner;
    if (!region || !region->alive) return 0;
    owner = region->owner_civ;
    return owner < 0 || owner >= civ_count || !civs[owner].alive;
}

void maritime_expansion_diagnostics(int civ_id, int resource_score, MaritimeExpansionDiagnostics *out) {
    ProfilerCallTrace trace = profiler_call_begin();
    int has_port;
    int sea_stability;
    int pressure;
    int i;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) {
        profiler_call_end("maritime_expansion_diagnostics", civ_id, -1, trace);
        return;
    }
    if (diag_cache_hit(civ_id, resource_score, out)) {
        profiler_call_end("maritime_expansion_diagnostics", civ_id, -1, trace);
        return;
    }
    has_port = diag_civ_has_port(civ_id);
    sea_stability = technology_deep_sea_stability(civ_id);
    pressure = max(population_pressure_for_civ(civ_id) / 8, clamp(18 - resource_score, 0, 14));
    if (!has_port) out->blocked_no_port = 1;
    for (i = 0; i < region_count; i++) {
        const NaturalRegion *region = regions_get(i);
        int sea_x;
        int sea_y;
        int sea_region;
        int direct = 0;
        int score;
        int threshold;

        if (!diag_region_claimable(region) || !region->has_port_site) continue;
        out->port_candidate_regions++;
        if (region->capital_x < 0 || region->port_x < 0 || region->port_y < 0) continue;
        if (city_at(region->capital_x, region->capital_y) >= 0) {
            out->blocked_city_at_capital++;
            continue;
        }
        if (!ports_find_nearby_sea_entry(region->port_x, region->port_y, &sea_x, &sea_y)) {
            out->blocked_no_sea_entry++;
            continue;
        }
        sea_region = ports_tile_shallow_region_near_land(sea_x, sea_y);
        if (!has_port) continue;
        if (diag_civ_port_in_region(civ_id, sea_region, sea_x, sea_y, &direct)) {
            out->shallow_reachable_regions++;
            score = diag_target_score(civ_id, region, pressure) - direct / 3;
            threshold = 116 - pressure * 5 - civs[civ_id].logistics * 4 -
                        civs[civ_id].commerce * 3 - civs[civ_id].expansion * 3;
            if (sea_stability <= 0) threshold += 10;
            if (city_count >= MAX_CITIES) out->blocked_city_cap++;
            else if (score < threshold) out->blocked_low_score++;
            else {
                out->maritime_reachable_regions++;
            }
        } else {
            out->blocked_same_shallow_region++;
        }
        if (sea_stability > 0 && diag_nearest_port_distance(civ_id, sea_x, sea_y, &direct) &&
            direct <= DIAG_MAX_ROUTE_DISTANCE) {
            out->deep_reachable_regions++;
        }
    }
    diag_cache_store(civ_id, resource_score, out);
    profiler_call_end("maritime_expansion_diagnostics", civ_id, -1, trace);
}
