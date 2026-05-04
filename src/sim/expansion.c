#include "expansion.h"

#include "sim/maritime.h"
#include "sim/simulation.h"
#include "world/terrain_query.h"

#include <stdlib.h>

static int owned_land_count_near(int civ_id, int x, int y, int radius, int unorganized_only) {
    int count = 0;
    int dy;
    int dx;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (dx * dx + dy * dy > radius * radius) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (world[ny][nx].owner != civ_id || !is_land(world[ny][nx].geography)) continue;
            if (unorganized_only && world[ny][nx].province_id >= 0) continue;
            count++;
        }
    }
    return count;
}

static int nearest_city_for_civ(int civ_id, int x, int y, int *dist_sq) {
    int i;
    int best = MAP_W * MAP_W + MAP_H * MAP_H;
    int best_city = -1;

    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        int dist;
        if (!cities[i].alive || cities[i].owner != civ_id) continue;
        dx = x - cities[i].x;
        dy = y - cities[i].y;
        dist = dx * dx + dy * dy;
        if (dist < best) {
            best = dist;
            best_city = i;
        }
    }
    if (dist_sq) *dist_sq = best;
    return best_city;
}

static int nearest_city_distance_sq_for_civ(int civ_id, int x, int y) {
    int distance_sq;
    nearest_city_for_civ(civ_id, x, y, &distance_sq);
    return distance_sq;
}

static int admin_radius_for_city(int city_id) {
    int civ_id;
    int radius;

    if (city_id < 0 || city_id >= city_count) return 0;
    civ_id = cities[city_id].owner;
    if (civ_id < 0 || civ_id >= civ_count) return cities[city_id].radius + 4;
    radius = cities[city_id].radius + 4 + civs[civ_id].governance / 4 + civs[civ_id].logistics / 5;
    return clamp(radius, cities[city_id].radius + 3, cities[city_id].radius + 10);
}

static int create_expansion_outpost(int civ_id, int x, int y, int target_score, int resource_pressure) {
    TerrainStats stats;
    int threshold;
    int population;
    int city_id;

    if (city_count >= MAX_CITIES) return -1;
    if (city_at(x, y) >= 0) return -1;
    if (!world_city_site_has_room(x, y, civ_id, 4)) return -1;

    threshold = 120 - resource_pressure * 4 - civs[civ_id].logistics * 2 - civs[civ_id].governance;
    if (target_score < threshold) return -1;

    stats = tile_stats(x, y);
    population = 28 + stats.food * 3 + stats.water * 3 + stats.pop_capacity * 3 +
                 stats.money * 2 + resource_pressure * 2 + rnd(30);
    city_id = world_create_city(civ_id, x, y, population, 0);
    if (city_id >= 0) world_claim_city_region(city_id, civ_id);
    return city_id;
}

static int apply_expansion_claim(int civ_id, int x, int y, int target_score, int resource_pressure) {
    int distance_sq;
    int nearest_city = nearest_city_for_civ(civ_id, x, y, &distance_sq);

    if (nearest_city >= 0) {
        int admin_radius = admin_radius_for_city(nearest_city);
        if (distance_sq <= admin_radius * admin_radius) {
            world[y][x].owner = civ_id;
            world[y][x].province_id = nearest_city;
            world_invalidate_region_cache();
            return nearest_city;
        }
    }

    if (create_expansion_outpost(civ_id, x, y, target_score, resource_pressure) >= 0) return -2;

    world[y][x].owner = civ_id;
    world[y][x].province_id = -1;
    world_invalidate_region_cache();
    return -1;
}

static int expansion_target_score(int civ_id, int x, int y, int resource_pressure) {
    TerrainStats stats = tile_stats(x, y);
    CountrySummary country = summarize_country(civ_id);
    int score = world_terrain_resource_value(stats);
    int habitability_escape = clamp(4 - country.habitability, 0, 4);
    int food_need = clamp(6 - country.food, 0, 6);
    int herd_need = clamp(5 - country.livestock, 0, 5);
    int wood_need = clamp(5 - country.wood, 0, 5);
    int stone_need = clamp(5 - country.stone, 0, 5);
    int ore_need = clamp(5 - country.minerals, 0, 5);
    int water_need = clamp(6 - country.water, 0, 6);

    score += stats.food * food_need * 3 + stats.livestock * herd_need * 2;
    score += stats.wood * wood_need * 2 + stats.stone * stone_need * 2 + stats.minerals * ore_need * 3;
    score += stats.water * water_need * 3 + stats.money * 4 + stats.pop_capacity * 3;
    score += resource_pressure * 8;
    score += habitability_escape * clamp(stats.habitability - country.habitability, 0, 6) * 12;
    if (world[y][x].river) score += 24;
    if (world_is_coastal_land_tile(x, y)) score += 12;
    if (world[y][x].temperature >= 35 && world[y][x].temperature <= 76 && stats.water >= 3) score += 16;
    if (world_nearby_enemy_border(civ_id, x, y, 1)) score -= 95;
    score -= world_tile_cost(x, y) * 9;
    score += rnd(20);
    return score;
}

static int pick_expansion_target(int civ_id, int resource_pressure, int *to_x, int *to_y, int *target_score) {
    static const int dirs[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
    };
    int tries;
    int best_score = -1000000;
    int found = 0;

    for (tries = 0; tries < 9500; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        int d = rnd(8);
        int nx = x + dirs[d][0];
        int ny = y + dirs[d][1];
        int score;

        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        if (world[y][x].owner != civ_id) continue;
        if (!is_land(world[ny][nx].geography)) continue;
        if (world[ny][nx].owner != -1) continue;
        score = expansion_target_score(civ_id, nx, ny, resource_pressure);
        if (score > best_score) {
            best_score = score;
            *to_x = nx;
            *to_y = ny;
            found = 1;
        }
    }
    if (found && target_score) *target_score = best_score;
    return found;
}

static int city_foundation_score(int civ_id, int x, int y, int resource_pressure) {
    TerrainStats stats = tile_stats(x, y);
    int owned = owned_land_count_near(civ_id, x, y, 6, 1);
    int distance_sq = nearest_city_distance_sq_for_civ(civ_id, x, y);
    int distance_units = distance_sq / (CITY_MIN_DISTANCE * CITY_MIN_DISTANCE);
    int score;

    if (owned < 18) return -1000000;
    if (distance_sq < CITY_MIN_DISTANCE * CITY_MIN_DISTANCE) return -1000000;
    score = world_terrain_resource_value(stats) + owned * 6 + clamp(distance_units, 0, 7) * 9;
    score += stats.water * 5 + stats.food * 4 + stats.pop_capacity * 4 + stats.money * 3;
    score += resource_pressure * 7;
    if (world[y][x].river) score += 24;
    if (world_is_coastal_land_tile(x, y)) score += 18;
    if (world_nearby_enemy_border(civ_id, x, y, 2)) score -= 100;
    score -= world_tile_cost(x, y) * 7;
    return score;
}

static void maybe_found_city(int civ_id, int resource_pressure) {
    int tries;
    int best_x = -1;
    int best_y = -1;
    int best_score = -1000000;
    int foundation_chance;
    int threshold;

    if (city_count >= MAX_CITIES || !civs[civ_id].alive) return;
    if (civs[civ_id].territory < 22) return;
    foundation_chance = clamp(18 + resource_pressure * 5 + civs[civ_id].logistics * 2 +
                              civs[civ_id].governance + civs[civ_id].production, 8, 86);
    if (rnd(100) >= foundation_chance) return;

    for (tries = 0; tries < 9000; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        int score;

        if (world[y][x].owner != civ_id) continue;
        if (!is_land(world[y][x].geography)) continue;
        if (world[y][x].province_id != -1) continue;
        if (city_at(x, y) >= 0) continue;
        if (!world_city_site_has_room(x, y, civ_id, 4)) continue;
        score = city_foundation_score(civ_id, x, y, resource_pressure);
        if (score > best_score) {
            best_score = score;
            best_x = x;
            best_y = y;
        }
    }

    threshold = 132 - civs[civ_id].governance * 3 - civs[civ_id].production * 3 - resource_pressure * 4;
    if (best_x >= 0 && best_score > threshold) {
        TerrainStats stats = tile_stats(best_x, best_y);
        int city_id = world_create_city(civ_id, best_x, best_y,
                                        32 + stats.food * 3 + stats.water * 3 +
                                        stats.pop_capacity * 3 + rnd(35), 0);
        if (city_id >= 0) world_claim_city_region(city_id, civ_id);
    }
}

static void resolve_expansion(int civ_id, int resource_score, char *log, size_t log_size) {
    int tx;
    int ty;
    Civilization *civ = &civs[civ_id];
    CountrySummary country = summarize_country(civ_id);
    int habitability_escape = clamp(4 - country.habitability, 0, 4);
    int drive;
    int cost;
    int target_score = 0;
    int resource_pressure = clamp(18 - resource_score, 0, 14);

    if (!civ->alive) return;
    if (!pick_expansion_target(civ_id, resource_pressure, &tx, &ty, &target_score)) return;

    cost = world_tile_cost(tx, ty);
    drive = civ->logistics * 6 + civ->governance * 2 + civ->cohesion + civ->adaptation * 2 + civ->expansion * 3 +
            resource_pressure * 7 + habitability_escape * 9 + target_score / 6 + rnd(40) +
            civ->population / 180 - cost * 4;

    if (drive > 58 + clamp(resource_score - 24, 0, 12) - habitability_escape * 3) {
        int province_result = apply_expansion_claim(civ_id, tx, ty, target_score, resource_pressure);
        if (province_result == -2) append_log(log, log_size, "%s founded a frontier province. ", civ->name);
        else append_log(log, log_size, "%s settled new land. ", civ->name);
    }
}

void expansion_update_civilization(int civ_id, int resource_score, char *log, size_t log_size) {
    int attempts;
    int resource_pressure;

    if (!civs[civ_id].alive) return;
    resource_pressure = clamp(18 - resource_score, 0, 14);
    attempts = 1 + civs[civ_id].expansion / 4 + civs[civ_id].logistics / 5 + resource_pressure / 4;
    if (resource_score > 26 && attempts > 1 && rnd(100) < 45) attempts--;
    while (attempts-- > 0) resolve_expansion(civ_id, resource_score, log, log_size);
    maybe_found_city(civ_id, resource_pressure);
    maritime_try_overseas_expansion(civ_id, resource_score, log, log_size);
}
