#include "plague.h"

#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/simulation.h"
#include "world/terrain_query.h"

#include <stdlib.h>
#include <string.h>

#define PLAGUE_MIN_DURATION 4
#define PLAGUE_MAX_DURATION 18
#define PLAGUE_LOCAL_RADIUS 88

static PlagueState city_plagues[MAX_CITIES];
static int route_exposure[MAX_MARITIME_ROUTES];

static int valid_city(int city_id) {
    return city_id >= 0 && city_id < city_count && cities[city_id].alive;
}

static int manhattan_city_distance(int a, int b) {
    return abs(cities[a].x - cities[b].x) + abs(cities[a].y - cities[b].y);
}

static void raise_city_immunity(int city_id, int amount) {
    if (!valid_city(city_id)) return;
    city_plagues[city_id].immunity = clamp(city_plagues[city_id].immunity + amount, 0, 10);
}

int plague_seed_city(int city_id, int severity, int months) {
    PlagueState *state;

    if (!valid_city(city_id) || cities[city_id].population <= 0) return 0;
    state = &city_plagues[city_id];
    if (state->active) {
        state->severity = clamp(state->severity + severity / 3, 1, 10);
        state->months_left = clamp(state->months_left + months / 3, 1, PLAGUE_MAX_DURATION);
        world_visual_revision++;
        return 1;
    }
    state->active = 1;
    state->infected = 1;
    state->severity = clamp(severity - state->immunity / 3, 1, 10);
    state->months_left = clamp(months, PLAGUE_MIN_DURATION, PLAGUE_MAX_DURATION);
    state->origin_city = city_id;
    state->age_months = 0;
    world_visual_revision++;
    return 1;
}

static int nearby_plague_pressure(int city_id) {
    int i;
    int pressure = 0;

    for (i = 0; i < city_count; i++) {
        int distance;
        if (i == city_id || !city_plagues[i].active || !valid_city(i)) continue;
        distance = manhattan_city_distance(city_id, i);
        if (distance > PLAGUE_LOCAL_RADIUS) continue;
        pressure += clamp(city_plagues[i].severity * 3 - distance / 8, 0, 30);
    }
    return pressure;
}

static int city_outbreak_risk(int city_id) {
    City *city;
    Civilization *civ = NULL;
    PopulationSummary pop;
    TerrainStats stats;
    int risk;

    if (!valid_city(city_id) || city_plagues[city_id].active) return 0;
    city = &cities[city_id];
    pop = population_city_summary(city_id);
    stats = tile_stats(city->x, city->y);
    if (pop.total <= 20) return 0;
    if (city->owner >= 0 && city->owner < civ_count) civ = &civs[city->owner];

    risk = 2 + pop.total / 700 + clamp(pop.pressure - 95, 0, 80) / 5;
    risk += clamp(5 - stats.water, 0, 5) * 4;
    risk += clamp(5 - stats.habitability, 0, 5) * 3;
    risk += city->port ? 5 : 0;
    risk += nearby_plague_pressure(city_id);
    if (civ) risk += civ->disorder * 3 + civ->disorder_plague * 2;
    risk -= city_plagues[city_id].immunity * 5;
    return clamp(risk, 0, 120);
}

int plague_seed_random_outbreak(void) {
    int i;
    int best_city = -1;
    int best_score = 0;

    for (i = 0; i < city_count; i++) {
        int score = city_outbreak_risk(i) + rnd(24);
        if (score > best_score) {
            best_score = score;
            best_city = i;
        }
    }
    if (best_city < 0 || best_score < 16) return 0;
    return plague_seed_city(best_city, 2 + best_score / 18 + rnd(3), PLAGUE_MIN_DURATION + rnd(9));
}

void plague_reset(void) {
    memset(city_plagues, 0, sizeof(city_plagues));
    memset(route_exposure, 0, sizeof(route_exposure));
}

static void try_infect_city(int source_city, int target_city, int chance, int route_id) {
    int immunity;

    if (!valid_city(source_city) || !valid_city(target_city) || city_plagues[target_city].active) return;
    immunity = city_plagues[target_city].immunity;
    chance = clamp(chance - immunity * 6, 0, 65);
    if (rnd(100) >= chance) return;
    plague_seed_city(target_city, clamp(city_plagues[source_city].severity - 1 + rnd(3), 1, 9),
                     PLAGUE_MIN_DURATION + rnd(8));
    if (route_id >= 0 && route_id < MAX_MARITIME_ROUTES) {
        route_exposure[route_id] = clamp(route_exposure[route_id] + city_plagues[source_city].severity + 2, 0, 10);
        world_visual_revision++;
    }
}

static void spread_locally_from_city(int source_city) {
    int i;
    int source_owner = cities[source_city].owner;
    int severity = city_plagues[source_city].severity;

    for (i = 0; i < city_count; i++) {
        int distance;
        int chance;
        if (i == source_city || !valid_city(i)) continue;
        distance = manhattan_city_distance(source_city, i);
        if (distance > PLAGUE_LOCAL_RADIUS) continue;
        chance = severity * 4 + clamp(PLAGUE_LOCAL_RADIUS - distance, 0, PLAGUE_LOCAL_RADIUS) / 7;
        if (cities[i].owner == source_owner) chance += 5;
        if (cities[source_city].port && cities[i].port) chance += 3;
        try_infect_city(source_city, i, chance, -1);
    }
}

static void spread_by_maritime_from_city(int source_city) {
    int i;
    int severity = city_plagues[source_city].severity;

    for (i = 0; i < maritime_route_count; i++) {
        MaritimeRoute *route = &maritime_routes[i];
        int target = -1;
        int chance;

        if (!route->active) continue;
        if (route->from_city == source_city) target = route->to_city;
        else if (route->to_city == source_city) target = route->from_city;
        if (!valid_city(target)) continue;
        route_exposure[i] = clamp(route_exposure[i] + severity / 2, 0, 10);
        world_visual_revision++;
        chance = severity * 3 + clamp(42 - route->distance / 10, 0, 34);
        if (cities[target].owner == cities[source_city].owner) chance += 4;
        try_infect_city(source_city, target, chance, i);
    }
}

static int decay_route_exposure(void) {
    int i;
    int changed = 0;

    for (i = 0; i < MAX_MARITIME_ROUTES; i++) {
        if (route_exposure[i] > 0) {
            route_exposure[i]--;
            changed = 1;
        }
    }
    return changed;
}

static void decay_city_immunity(void) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (!valid_city(i) || city_plagues[i].active || city_plagues[i].immunity <= 0) continue;
        if (rnd(100) < 12) city_plagues[i].immunity--;
    }
}

static void update_active_city(int city_id, int active_by_civ[MAX_CIVS],
                               int severity_by_civ[MAX_CIVS], int deaths_by_civ[MAX_CIVS],
                               int *any_change) {
    PlagueState *state = &city_plagues[city_id];
    int owner = cities[city_id].owner;
    int deaths;

    if (!state->active || !valid_city(city_id)) return;
    deaths = population_apply_city_plague(city_id, state->severity);
    state->deaths_total += deaths;
    state->age_months++;
    state->months_left--;
    if (owner >= 0 && owner < civ_count) {
        active_by_civ[owner]++;
        severity_by_civ[owner] += state->severity;
        deaths_by_civ[owner] += deaths;
    }
    if (deaths > 0) *any_change = 1;
    spread_locally_from_city(city_id);
    spread_by_maritime_from_city(city_id);
    if (state->age_months <= 2 && rnd(100) < 28) state->severity = clamp(state->severity + 1, 1, 10);
    else if (state->months_left <= 5 || rnd(100) < 35) state->severity = clamp(state->severity - 1, 0, 10);
    if (state->months_left <= 0 || state->severity <= 0 || cities[city_id].population <= 0) {
        state->active = 0;
        state->infected = 0;
        state->severity = 0;
        state->months_left = 0;
        raise_city_immunity(city_id, 3 + state->age_months / 4);
    }
}

static void apply_plague_disorder(int active_by_civ[MAX_CIVS], int severity_by_civ[MAX_CIVS],
                                  int deaths_by_civ[MAX_CIVS]) {
    int i;

    for (i = 0; i < civ_count; i++) {
        int pressure;
        if (!civs[i].alive || active_by_civ[i] <= 0) continue;
        pressure = active_by_civ[i] + severity_by_civ[i] / 3 + deaths_by_civ[i] / 600;
        civs[i].disorder_plague = clamp(pressure, civs[i].disorder_plague, 10);
        civs[i].disorder = clamp(civs[i].disorder + clamp(pressure / 4, 0, 2), 0, 10);
    }
}

void plague_update_month(void) {
    int active_by_civ[MAX_CIVS] = {0};
    int severity_by_civ[MAX_CIVS] = {0};
    int deaths_by_civ[MAX_CIVS] = {0};
    int i;
    int any_change = 0;

    if (decay_route_exposure()) world_visual_revision++;
    decay_city_immunity();
    for (i = 0; i < city_count; i++) update_active_city(i, active_by_civ, severity_by_civ, deaths_by_civ, &any_change);
    apply_plague_disorder(active_by_civ, severity_by_civ, deaths_by_civ);
    if (rnd(1000) < 10) any_change |= plague_seed_random_outbreak();
    if (any_change) {
        population_sync_all();
        world_invalidate_region_cache();
    }
}

void plague_notify_migration(int from_city, int to_city, int migrants) {
    int chance;

    if (migrants <= 0 || !valid_city(from_city) || !valid_city(to_city)) return;
    if (!city_plagues[from_city].active || city_plagues[to_city].active) return;
    chance = city_plagues[from_city].severity * 7 + migrants * 3;
    try_infect_city(from_city, to_city, chance, -1);
}

void plague_notify_war_casualties(int civ_id, int casualties) {
    int tries;

    if (civ_id < 0 || civ_id >= civ_count || casualties <= 0 || !civs[civ_id].alive) return;
    for (tries = 0; tries < 6; tries++) {
        int city_id = rnd(city_count);
        int chance;
        if (!valid_city(city_id) || cities[city_id].owner != civ_id) continue;
        chance = clamp(casualties / 2 + civs[civ_id].disorder * 3 + city_outbreak_risk(city_id) / 4, 0, 55);
        if (rnd(100) < chance) {
            plague_seed_city(city_id, 3 + rnd(4), PLAGUE_MIN_DURATION + rnd(8));
            return;
        }
    }
}

int plague_city_active(int city_id) {
    return valid_city(city_id) && city_plagues[city_id].active;
}

int plague_city_severity(int city_id) {
    return plague_city_active(city_id) ? city_plagues[city_id].severity : 0;
}

int plague_city_deaths_total(int city_id) {
    return valid_city(city_id) ? city_plagues[city_id].deaths_total : 0;
}

int plague_city_months_left(int city_id) {
    return plague_city_active(city_id) ? city_plagues[city_id].months_left : 0;
}

int plague_tile_severity(int x, int y) {
    int city_id;

    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    city_id = world[y][x].province_id;
    return plague_city_severity(city_id);
}

int plague_civ_active_count(int civ_id) {
    int i;
    int count = 0;

    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < city_count; i++) {
        if (valid_city(i) && cities[i].owner == civ_id && city_plagues[i].active) count++;
    }
    return count;
}

int plague_civ_pressure(int civ_id) {
    int i;
    int pressure = 0;

    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < city_count; i++) {
        if (valid_city(i) && cities[i].owner == civ_id && city_plagues[i].active) {
            pressure += city_plagues[i].severity + city_plagues[i].deaths_total / 600;
        }
    }
    return clamp(pressure, 0, 10);
}

int plague_civ_deaths_total(int civ_id) {
    int i;
    int total = 0;

    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < city_count; i++) {
        if (valid_city(i) && cities[i].owner == civ_id) total += city_plagues[i].deaths_total;
    }
    return total;
}

int plague_civ_peak_severity(int civ_id) {
    int i;
    int peak = 0;

    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < city_count; i++) {
        if (valid_city(i) && cities[i].owner == civ_id && city_plagues[i].severity > peak) {
            peak = city_plagues[i].severity;
        }
    }
    return peak;
}

int plague_civ_months_left(int civ_id) {
    int i;
    int left = 0;

    if (civ_id < 0 || civ_id >= civ_count) return 0;
    for (i = 0; i < city_count; i++) {
        if (valid_city(i) && cities[i].owner == civ_id && city_plagues[i].months_left > left) {
            left = city_plagues[i].months_left;
        }
    }
    return left;
}

int plague_route_exposure(int route_id) {
    MaritimeRoute *route;
    int exposure;

    if (route_id < 0 || route_id >= maritime_route_count || route_id >= MAX_MARITIME_ROUTES) return 0;
    route = &maritime_routes[route_id];
    if (!route->active) return 0;
    exposure = route_exposure[route_id];
    exposure = clamp(exposure + plague_city_severity(route->from_city) / 2, 0, 10);
    exposure = clamp(exposure + plague_city_severity(route->to_city) / 2, 0, 10);
    return exposure;
}
