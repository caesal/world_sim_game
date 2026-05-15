#include "maritime.h"
#include "core/dirty_flags.h"
#include "core/profiler.h"
#include "sim/diplomacy.h"
#include "sim/disorder.h"
#include "sim/population.h"
#include "sim/plague.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/sea_lanes.h"
#include "sim/simulation.h"
#include "sim/technology.h"
#include <stdlib.h>
#include <string.h>
#define MARITIME_TARGET_KEEP 8
#define MARITIME_PATH_CHECKS_PER_CALL 2
static int maritime_routes_dirty = 1;
static int route_revision = 1;
static int ownership_revision = 1;
static unsigned char overseas_target_cursor[MAX_CIVS];
typedef struct {
    int x;
    int y;
    int sea_x;
    int sea_y;
    int region;
    int land_region_id;
    int score;
    int direct_distance;
    int route_type;
} MaritimeTarget;
void maritime_reset(void) {
    memset(maritime_routes, 0, sizeof(maritime_routes));
    memset(overseas_target_cursor, 0, sizeof(overseas_target_cursor));
    maritime_route_count = 0;
    maritime_routes_dirty = 1;
    route_revision++;
    dirty_mark_maritime();
}
void maritime_mark_routes_dirty(void) {
    maritime_routes_dirty = 1; route_revision++; dirty_mark_maritime(); diplomacy_mark_contacts_dirty();
}
void maritime_mark_ownership_dirty(void) {
    maritime_routes_dirty = 1; ownership_revision++; dirty_mark_maritime(); diplomacy_mark_contacts_dirty();
}
int maritime_route_revision(void) { return route_revision; }
int maritime_ownership_revision(void) { return ownership_revision; }
void maritime_ensure_routes(void) { if (maritime_routes_dirty) maritime_rebuild_routes(); }
void maritime_rebuild_routes(void) {
    const SeaLane *lanes;
    int lane_count;
    int i;
    memset(maritime_routes, 0, sizeof(maritime_routes));
    maritime_route_count = 0;
    ports_refresh_city_regions();
    lanes = sea_lanes_get(&lane_count);
    for (i = 0; i < lane_count && maritime_route_count < MAX_MARITIME_ROUTES; i++) {
        MaritimeRoute *route;
        if (!lanes[i].active || lanes[i].point_count < 2) continue;
        route = &maritime_routes[maritime_route_count++];
        memset(route, 0, sizeof(*route));
        route->active = 1;
        route->from_city = lanes[i].from_city;
        route->to_city = lanes[i].to_city;
        route->sea_region = lanes[i].network_a;
        route->distance = lanes[i].point_count;
        route->point_count = min(lanes[i].point_count, MAX_MARITIME_ROUTE_POINTS);
        memcpy(route->points, lanes[i].points, (size_t)route->point_count * sizeof(route->points[0]));
    }
    maritime_routes_dirty = 0;
    dirty_mark_maritime();
}
int maritime_route_between_cities(int city_a, int city_b, int *distance) {
    return sea_lanes_connected(city_a, city_b, distance);
}
void maritime_update_migration(void) {
    const SeaLane *lanes;
    int lane_count;
    int i;
    int changed = 0;
    lanes = sea_lanes_get(&lane_count);
    for (i = 0; i < lane_count; i++) {
        const SeaLane *lane = &lanes[i];
        City *a;
        City *b;
        City *donor;
        City *receiver;
        int diff;
        int migrants;
        if (!lane->active) continue;
        a = &cities[lane->from_city];
        b = &cities[lane->to_city];
        if (!a->alive || !b->alive || a->owner < 0 || a->owner != b->owner) continue;
        donor = a->population >= b->population ? a : b;
        receiver = donor == a ? b : a;
        diff = donor->population - receiver->population;
        if (diff < 40) continue;
        migrants = clamp(diff / (130 + lane->point_count * 2), 1, 5);
        {
            int from_city = (int)(donor - cities);
            int to_city = (int)(receiver - cities);
            int moved = population_migrate_between_cities(from_city, to_city, migrants);
            if (moved > 0) {
                plague_notify_migration(from_city, to_city, moved);
                changed = 1;
            }
        }
    }
    if (changed) world_invalidate_population_cache();
}
int maritime_has_contact(int civ_a, int civ_b) {
    return sea_lanes_has_contact(civ_a, civ_b);
}
int maritime_trade_bonus(int civ_a, int civ_b) {
    return sea_lanes_trade_bonus(civ_a, civ_b);
}
static void keep_maritime_target(MaritimeTarget targets[MARITIME_TARGET_KEEP], MaritimeTarget candidate) {
    int i;
    for (i = 0; i < MARITIME_TARGET_KEEP; i++) {
        if (candidate.score > targets[i].score) {
            int move;
            for (move = MARITIME_TARGET_KEEP - 1; move > i; move--) targets[move] = targets[move - 1];
            targets[i] = candidate;
            return;
        }
    }
}
static int target_region_score(int civ_id, const NaturalRegion *region, int pressure) {
    int score;
    if (!region) return -1000000;
    score = region->development_score + region->cradle_score / 2;
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
static int region_claimable_by_alive_civ(const NaturalRegion *region) {
    int owner;
    if (!region || !region->alive) return 0;
    owner = region->owner_civ;
    return owner < 0 || owner >= civ_count || !civs[owner].alive;
}
static int collect_overseas_targets(int civ_id, int pressure, int allow_deep,
                                    MaritimeTarget targets[MARITIME_TARGET_KEEP]) {
    int i;
    int found = 0;
    const RoutePortNode *nodes;
    int node_count;
    nodes = route_potential_nodes(&node_count);
    for (i = 0; i < MARITIME_TARGET_KEEP; i++) targets[i].score = -1000000;
    for (i = 0; i < region_count; i++) {
        const NaturalRegion *region = regions_get(i);
        MaritimeTarget candidate;
        int node_index;
        int direct;
        int route_type;
        if (!region_claimable_by_alive_civ(region) || !region->has_port_site) continue;
        if (region->capital_x < 0 || region->port_x < 0 || region->port_y < 0) continue;
        if (city_at(region->capital_x, region->capital_y) >= 0) continue;
        node_index = route_potential_region_node(i);
        if (node_index < 0 || node_index >= node_count) continue;
        if (!route_potential_region_reachable(civ_id, i, allow_deep, &direct, &route_type)) continue;
        candidate.land_region_id = i;
        candidate.x = region->capital_x;
        candidate.y = region->capital_y;
        candidate.sea_x = nodes[node_index].sea_x;
        candidate.sea_y = nodes[node_index].sea_y;
        candidate.region = nodes[node_index].network;
        candidate.direct_distance = direct;
        candidate.route_type = route_type;
        candidate.score = target_region_score(civ_id, region, pressure) - direct / 3 +
                          (route_type == ROUTE_POTENTIAL_SHALLOW ? 18 : 0) + rnd(24);
        keep_maritime_target(targets, candidate);
        found = 1;
    }
    return found;
}
static int path_distance_to_target(int civ_id, MaritimeTarget target, int *distance, int *checks_left) {
    (void)civ_id;
    if (checks_left && *checks_left <= 0) return 0;
    if (checks_left) (*checks_left)--;
    if (target.direct_distance <= 0) return 0;
    if (distance) *distance = target.direct_distance;
    return 1;
}
void maritime_try_overseas_expansion(int civ_id, int resource_score, char *log, size_t log_size) {
    ProfilerCallTrace trace = profiler_call_begin();
    MaritimeTarget targets[MARITIME_TARGET_KEEP];
    int pressure = max(population_pressure_for_civ(civ_id) / 8, clamp(18 - resource_score, 0, 14));
    int sea_stability = technology_deep_sea_stability(civ_id);
    int i;
    int chance;
    int checks_left = MARITIME_PATH_CHECKS_PER_CALL;
    int start = civ_id >= 0 && civ_id < MAX_CIVS ? overseas_target_cursor[civ_id] % MARITIME_TARGET_KEEP : 0;
#define RETURN_OVERSEAS() do { profiler_call_end("maritime_try_overseas_expansion", civ_id, -1, trace); return; } while (0)
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) RETURN_OVERSEAS();
    maritime_ensure_routes();
    chance = clamp(12 + pressure * 4 + civs[civ_id].logistics * 3 +
                   civs[civ_id].commerce * 2 + civs[civ_id].expansion * 3, 8, 82);
    if (sea_stability <= 0) chance = chance * 3 / 4;
    else chance += sea_stability / 10;
    if (rnd(100) >= chance) RETURN_OVERSEAS();
    if (!collect_overseas_targets(civ_id, pressure, sea_stability > 0, targets)) {
        if (civ_id >= 0 && civ_id < MAX_CIVS) overseas_target_cursor[civ_id] = 0;
        RETURN_OVERSEAS();
    }
    for (i = 0; i < MARITIME_TARGET_KEEP; i++) {
        int index = (start + i) % MARITIME_TARGET_KEEP;
        int distance;
        int threshold;
        int city_cap_fallback;
        if (targets[index].score <= -1000000) continue;
        if (checks_left <= 0) {
            if (civ_id >= 0 && civ_id < MAX_CIVS) overseas_target_cursor[civ_id] = (unsigned char)index;
            append_log(log, log_size, "[Expansion] Maritime expansion yielded after path budget. ");
            RETURN_OVERSEAS();
        }
        if (!path_distance_to_target(civ_id, targets[index], &distance, &checks_left)) {
            if (checks_left <= 0 && civ_id >= 0 && civ_id < MAX_CIVS) {
                overseas_target_cursor[civ_id] = (unsigned char)index;
                append_log(log, log_size, "[Expansion] Maritime expansion yielded after path budget. ");
                RETURN_OVERSEAS();
            }
            continue;
        }
        targets[index].score -= distance / 2;
        threshold = 116 - pressure * 5 - civs[civ_id].logistics * 4 -
                    civs[civ_id].commerce * 3 - civs[civ_id].expansion * 3;
        if (sea_stability <= 0) threshold += 10;
        if (targets[index].score < threshold) continue;
        if (sea_stability > 0 && rnd(100) >= sea_stability) {
            int payload = clamp(max(600, civs[civ_id].population / 70), 600, 35000);
            population_apply_casualties(civ_id, payload);
            disorder_add_migration_pressure(civ_id, payload / 2500 + 1);
            event_log_push_structured(EVENT_TYPE_DEEP_SEA_ROUTE_FAILED, EVENT_SEVERITY_WARNING,
                                      civ_id, -1, targets[index].land_region_id, -1, payload, 0, NULL);
            continue;
        }
        city_cap_fallback = city_count >= MAX_CITIES;
        if (!regions_claim_for_civ(targets[index].land_region_id, civ_id, -1, 1)) {
            if (city_cap_fallback) append_log(log, log_size, "[Expansion] Island claim failed: city cap and no owner admin city. ");
            continue;
        }
        if (city_cap_fallback) append_log(log, log_size, "[Expansion] Claimed island as dependency of nearest owned city. ");
        if (civ_id >= 0 && civ_id < MAX_CIVS) overseas_target_cursor[civ_id] = (unsigned char)((index + 1) % MARITIME_TARGET_KEEP);
        if (sea_stability > 0 && !civs[civ_id].deep_sea_route_unlocked_event_done) {
            disorder_relieve(civ_id, 25);
            civs[civ_id].deep_sea_route_unlocked_event_done = 1;
        }
        event_log_push_structured(EVENT_TYPE_EXPANSION_CLAIMED, EVENT_SEVERITY_INFO,
                                  civ_id, -1, targets[index].land_region_id, -1, 0, 1, NULL);
        RETURN_OVERSEAS();
    }
    RETURN_OVERSEAS();
#undef RETURN_OVERSEAS
}
