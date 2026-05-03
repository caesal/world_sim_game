#include "simulation.h"

#include "world_gen.h"
#include "sim/diplomacy.h"
#include "sim/expansion.h"
#include "sim/war.h"
#include "world/ports.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void invalidate_region_cache(void);
static void repair_city_location(int city_id, int min_border_distance);

static int province_cache[MAP_H][MAP_W];
static int province_cache_dirty = 1;
static RegionSummary region_summary_cache[MAX_CITIES];
static int region_summary_dirty = 1;
static CountrySummary country_summary_cache[MAX_CIVS];
static int country_summary_dirty = 1;

typedef struct {
    int x;
    int y;
} ProvinceFrontierNode;

static int province_claim_cost[MAP_H][MAP_W];
static int province_border_distance[MAP_H][MAP_W];
static ProvinceFrontierNode province_frontier[MAP_W * MAP_H];

static void invalidate_region_cache(void) {
    province_cache_dirty = 1;
    region_summary_dirty = 1;
    country_summary_dirty = 1;
}

static int compute_city_for_tile_uncached(int x, int y) {
    int province_id;

    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return -1;
    province_id = world[y][x].province_id;
    if (province_id < 0 || province_id >= city_count || !cities[province_id].alive) return -1;
    return province_id;
}

static void rebuild_province_cache(void) {
    int x;
    int y;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            province_cache[y][x] = compute_city_for_tile_uncached(x, y);
        }
    }
    province_cache_dirty = 0;
    region_summary_dirty = 1;
}

static void ensure_province_cache(void) {
    if (province_cache_dirty) rebuild_province_cache();
}

static void rebuild_region_summary_cache(void) {
    int i;
    int x;
    int y;

    ensure_province_cache();
    memset(region_summary_cache, 0, sizeof(region_summary_cache));
    for (i = 0; i < MAX_CITIES; i++) region_summary_cache[i].city_id = i;
    for (i = 0; i < city_count; i++) {
        if (cities[i].alive) region_summary_cache[i].population = cities[i].population;
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int city_id = province_cache[y][x];
            TerrainStats stats;

            if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) continue;
            stats = tile_stats(x, y);
            region_summary_cache[city_id].tiles++;
            region_summary_cache[city_id].food += stats.food;
            region_summary_cache[city_id].livestock += stats.livestock;
            region_summary_cache[city_id].wood += stats.wood;
            region_summary_cache[city_id].stone += stats.stone;
            region_summary_cache[city_id].minerals += stats.minerals;
            region_summary_cache[city_id].water += stats.water;
            region_summary_cache[city_id].pop_capacity += stats.pop_capacity;
            region_summary_cache[city_id].money += stats.money;
            region_summary_cache[city_id].habitability += stats.habitability;
            region_summary_cache[city_id].attack += stats.attack;
            region_summary_cache[city_id].defense += stats.defense;
        }
    }

    for (i = 0; i < city_count; i++) {
        RegionSummary *summary = &region_summary_cache[i];
        if (summary->tiles <= 0) continue;
        summary->food /= summary->tiles;
        summary->livestock /= summary->tiles;
        summary->wood /= summary->tiles;
        summary->stone /= summary->tiles;
        summary->minerals /= summary->tiles;
        summary->water /= summary->tiles;
        summary->pop_capacity /= summary->tiles;
        summary->money /= summary->tiles;
        summary->habitability /= summary->tiles;
        summary->attack /= summary->tiles;
        summary->defense /= summary->tiles;
    }
    region_summary_dirty = 0;
}

static void recalculate_territory(void) {
    int i;
    int y;
    int x;

    for (i = 0; i < civ_count; i++) civs[i].territory = 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && owner < civ_count && civs[owner].alive) civs[owner].territory++;
        }
    }
    for (i = 0; i < civ_count; i++) {
        if (civs[i].territory <= 0 || civs[i].population <= 0) civs[i].alive = 0;
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && (owner >= civ_count || !civs[owner].alive)) world[y][x].owner = -1;
        }
    }
    for (i = 0; i < city_count; i++) {
        int owner = cities[i].owner;
        if (cities[i].alive && owner >= 0 && (owner >= civ_count || !civs[owner].alive)) {
            cities[i].alive = 0;
            cities[i].owner = -1;
            cities[i].capital = 0;
        }
    }
    invalidate_region_cache();
}


static int city_too_close_except(int x, int y, int min_distance, int except_city) {
    int i;
    int min_distance_sq = min_distance * min_distance;

    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        if (i == except_city) continue;
        if (!cities[i].alive) continue;
        dx = x - cities[i].x;
        dy = y - cities[i].y;
        if (dx * dx + dy * dy < min_distance_sq) return 1;
    }
    return 0;
}

static int city_too_close(int x, int y, int min_distance) {
    return city_too_close_except(x, y, min_distance, -1);
}

static int city_site_has_room(int x, int y, int owner, int radius) {
    int open_tiles = 0;
    int needed_tiles = 0;
    int dy;
    int dx;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (dx * dx + dy * dy > radius * radius) continue;
            needed_tiles++;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) continue;
            if (world[ny][nx].province_id >= 0) continue;
            if (owner < 0 && world[ny][nx].owner != -1) continue;
            if (owner >= 0 && world[ny][nx].owner != -1 && world[ny][nx].owner != owner) continue;
            open_tiles++;
        }
    }
    return open_tiles * 100 >= needed_tiles * 68;
}

static int find_empty_land(int *out_x, int *out_y) {
    int tries;

    for (tries = 0; tries < 12000; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        TerrainStats stats = tile_stats(x, y);
        if (is_land(world[y][x].geography) && world[y][x].owner == -1 && world[y][x].province_id == -1 &&
            stats.habitability > 1 && !city_too_close(x, y, CITY_MIN_DISTANCE) &&
            city_site_has_room(x, y, -1, 3)) {
            *out_x = x;
            *out_y = y;
            return 1;
        }
    }
    return 0;
}

static int city_radius_for_tile(int x, int y, int population) {
    TerrainStats stats = tile_stats(x, y);
    int resource_score = stats.food + stats.livestock + stats.water + stats.habitability + stats.wood / 2 + stats.minerals / 2;
    return clamp(2 + population / 180 + resource_score / 8, 3, 10);
}

static int province_entry_cost(int city_id, int x, int y, int from_x, int from_y) {
    TerrainStats stats = tile_stats(x, y);
    City *city = &cities[city_id];
    int cost;
    int elevation_step = abs(world[y][x].elevation - world[from_y][from_x].elevation) / 8;
    int elevation_from_city = abs(world[y][x].elevation - world[city->y][city->x].elevation) / 18;

    cost = 8 + world_tile_cost(x, y) + elevation_step + elevation_from_city;
    if (world[y][x].river) cost -= 2;
    if (world[y][x].geography == GEO_MOUNTAIN) cost += 8;
    else if (world[y][x].geography == GEO_HILL) cost += 3;
    else if (world[y][x].geography == GEO_BASIN) cost -= 1;
    if (stats.habitability >= 7) cost -= 2;
    if (stats.water >= 7) cost -= 1;
    cost += ((x * 17 + y * 31 + city_id * 13) % 7) - 3;
    return clamp(cost, 3, 28);
}

static int nearby_enemy_border(int owner, int x, int y, int radius) {
    int dy;
    int dx;

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            int other_owner;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            other_owner = world[ny][nx].owner;
            if (other_owner >= 0 && other_owner != owner) return 1;
        }
    }
    return 0;
}

static int province_tile_is_border(int city_id, int x, int y) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int i;

    for (i = 0; i < 4; i++) {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) return 1;
        if (world[ny][nx].province_id != city_id) return 1;
    }
    return 0;
}

static int compute_province_border_distance(int city_id) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int x;
    int y;
    int head = 0;
    int tail = 0;
    int max_distance = 0;

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) province_border_distance[y][x] = -1;
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].province_id != city_id) continue;
            if (!province_tile_is_border(city_id, x, y)) continue;
            province_border_distance[y][x] = 0;
            province_frontier[tail].x = x;
            province_frontier[tail].y = y;
            tail++;
        }
    }

    while (head < tail) {
        ProvinceFrontierNode node = province_frontier[head++];
        int i;
        int distance = province_border_distance[node.y][node.x] + 1;

        for (i = 0; i < 4; i++) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (world[ny][nx].province_id != city_id) continue;
            if (province_border_distance[ny][nx] >= 0) continue;
            province_border_distance[ny][nx] = distance;
            if (distance > max_distance) max_distance = distance;
            if (tail < MAP_W * MAP_H) {
                province_frontier[tail].x = nx;
                province_frontier[tail].y = ny;
                tail++;
            }
        }
    }
    return max_distance;
}

static int city_location_score(int city_id, int x, int y, int border_distance, int port_site) {
    City *city = &cities[city_id];
    TerrainStats stats = tile_stats(x, y);
    int score = border_distance * (port_site ? 10 : 36) + world_terrain_resource_value(stats);

    score += stats.water * 5 + stats.food * 5 + stats.pop_capacity * 4 + stats.habitability * 3;
    if (world[y][x].river) score += 18;
    if (world_is_coastal_land_tile(x, y)) score += port_site ? 34 : 5;
    score -= world_tile_cost(x, y) * 8;
    if (nearby_enemy_border(city->owner, x, y, 2)) score -= 90;
    return score;
}

static int select_city_site_in_province(int city_id, int min_border_distance, int port_only,
                                        int *out_x, int *out_y) {
    int x;
    int y;
    int best_score = -1000000;
    int found = 0;

    compute_province_border_distance(city_id);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int distance;
            int score;
            if (world[y][x].province_id != city_id) continue;
            distance = province_border_distance[y][x];
            if (distance < min_border_distance) continue;
            if (port_only && !world_is_coastal_land_tile(x, y)) continue;
            if (city_too_close_except(x, y, CITY_MIN_DISTANCE, city_id)) continue;
            score = city_location_score(city_id, x, y, distance, port_only);
            if (score > best_score) {
                best_score = score;
                *out_x = x;
                *out_y = y;
                found = 1;
            }
        }
    }
    return found;
}

static void repair_city_location(int city_id, int min_border_distance) {
    City *city;
    int x;
    int y;

    if (city_id < 0 || city_id >= city_count) return;
    city = &cities[city_id];
    if (!city->alive || city->port) return;
    if (!select_city_site_in_province(city_id, min_border_distance, 0, &x, &y) &&
        !select_city_site_in_province(city_id, 2, 0, &x, &y) &&
        !select_city_site_in_province(city_id, 1, 0, &x, &y)) {
        return;
    }
    city->x = x;
    city->y = y;
    city->radius = city_radius_for_tile(x, y, city->population);
}

static void make_city_name(char *buffer, size_t buffer_size, int owner, int x, int y, int capital) {
    const char *prefixes[] = {
        "North", "South", "East", "West", "High", "Low", "River", "Stone",
        "Green", "Red", "Gold", "Silver", "Old", "New", "Lake", "Hill"
    };
    const char *roots[] = {
        "ford", "mere", "hold", "watch", "vale", "crest", "harbor", "field",
        "march", "gate", "fall", "haven", "wick", "brook", "den", "reach"
    };

    if (capital && owner >= 0 && owner < civ_count) snprintf(buffer, buffer_size, "%s Capital", civs[owner].name);
    else snprintf(buffer, buffer_size, "%s%s Province", prefixes[(x * 3 + y) % 16], roots[(x + y * 5) % 16]);
}

static int create_city(int owner, int x, int y, int population, int capital) {
    City *city;

    if (city_count >= MAX_CITIES) return -1;
    if (city_too_close(x, y, CITY_MIN_DISTANCE)) return -1;
    city = &cities[city_count];
    city->alive = 1;
    city->owner = owner;
    make_city_name(city->name, sizeof(city->name), owner, x, y, capital);
    city->x = x;
    city->y = y;
    city->population = clamp(population, 1, MAX_POPULATION);
    city->radius = city_radius_for_tile(x, y, population);
    city->capital = capital;
    city->port = 0;
    city->port_region = -1;
    return city_count++;
}

static void claim_city_region(int city_id, int owner) {
    int x;
    int y;
    int has_existing_region = 0;
    int head = 0;
    int tail = 0;
    int budget;
    City *city;

    if (city_id < 0 || city_id >= city_count) return;
    city = &cities[city_id];
    if (!city->alive) return;

    city->owner = owner;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (world[y][x].province_id == city_id) {
                world[y][x].owner = owner;
                has_existing_region = 1;
            }
        }
    }
    if (has_existing_region) {
        invalidate_region_cache();
        return;
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) province_claim_cost[y][x] = -1;
    }

    budget = clamp(city->radius * 15 + tile_stats(city->x, city->y).habitability * 5 + city->population / 80, 64, 190);
    {
        int core_radius = clamp(city->radius / 2, 2, 3);
        int dy;
        int dx;

        for (dy = -core_radius; dy <= core_radius; dy++) {
            for (dx = -core_radius; dx <= core_radius; dx++) {
                int nx = city->x + dx;
                int ny = city->y + dy;
                int core_cost = (abs(dx) + abs(dy)) * 2;

                if (dx * dx + dy * dy > core_radius * core_radius) continue;
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                if (!is_land(world[ny][nx].geography)) continue;
                if (world[ny][nx].province_id >= 0 && world[ny][nx].province_id != city_id) continue;
                if (world[ny][nx].owner >= 0 && world[ny][nx].owner != owner) continue;
                world[ny][nx].owner = owner;
                world[ny][nx].province_id = city_id;
                province_claim_cost[ny][nx] = core_cost;
                province_frontier[tail].x = nx;
                province_frontier[tail].y = ny;
                tail++;
            }
        }
    }
    if (tail == 0) {
        province_claim_cost[city->y][city->x] = 0;
        province_frontier[tail].x = city->x;
        province_frontier[tail].y = city->y;
        tail++;
    }

    while (head < tail) {
        static const int dirs[8][2] = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1},
            {1, 1}, {-1, 1}, {1, -1}, {-1, -1}
        };
        ProvinceFrontierNode node = province_frontier[head++];
        int direction;

        world[node.y][node.x].owner = owner;
        world[node.y][node.x].province_id = city_id;
        for (direction = 0; direction < 8; direction++) {
            int nx = node.x + dirs[direction][0];
            int ny = node.y + dirs[direction][1];
            int next_cost;

            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) continue;
            if (world[ny][nx].province_id >= 0 && world[ny][nx].province_id != city_id) continue;
            if (world[ny][nx].owner >= 0 && world[ny][nx].owner != owner) continue;
            next_cost = province_claim_cost[node.y][node.x] + province_entry_cost(city_id, nx, ny, node.x, node.y);
            if (next_cost > budget) continue;
            if (province_claim_cost[ny][nx] >= 0 && province_claim_cost[ny][nx] <= next_cost) continue;
            province_claim_cost[ny][nx] = next_cost;
            if (tail < MAP_W * MAP_H) {
                province_frontier[tail].x = nx;
                province_frontier[tail].y = ny;
                tail++;
            }
        }
    }
    repair_city_location(city_id, city->capital ? 4 : 3);
    ports_maybe_make_city_port(city_id);
    invalidate_region_cache();
}

int city_at(int x, int y) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].x == x && cities[i].y == y) return i;
    }
    return -1;
}

int world_city_site_has_room(int x, int y, int owner, int radius) {
    return city_site_has_room(x, y, owner, radius);
}

int world_nearby_enemy_border(int owner, int x, int y, int radius) {
    return nearby_enemy_border(owner, x, y, radius);
}

int world_city_radius_for_tile(int x, int y, int population) {
    return city_radius_for_tile(x, y, population);
}

int world_select_city_site_in_province(int city_id, int min_border_distance, int port_only,
                                       int *out_x, int *out_y) {
    return select_city_site_in_province(city_id, min_border_distance, port_only, out_x, out_y);
}

int world_create_city(int owner, int x, int y, int population, int capital) {
    return create_city(owner, x, y, population, capital);
}

void world_claim_city_region(int city_id, int owner) {
    claim_city_region(city_id, owner);
}

void world_recalculate_territory(void) {
    recalculate_territory();
}

void world_invalidate_region_cache(void) {
    invalidate_region_cache();
}

int city_for_tile(int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return -1;
    ensure_province_cache();
    return province_cache[y][x];
}

RegionSummary summarize_city_region(int city_id) {
    RegionSummary summary;

    memset(&summary, 0, sizeof(summary));
    summary.city_id = city_id;
    if (city_id < 0 || city_id >= city_count) return summary;
    if (region_summary_dirty) rebuild_region_summary_cache();
    return region_summary_cache[city_id];
}

static void rebuild_country_summary_cache(void) {
    int x;
    int y;
    int i;

    memset(country_summary_cache, 0, sizeof(country_summary_cache));
    for (i = 0; i < civ_count; i++) {
        country_summary_cache[i].population = civs[i].population;
        country_summary_cache[i].territory = civs[i].territory;
    }
    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].owner >= 0 && cities[i].owner < civ_count) {
            country_summary_cache[cities[i].owner].cities++;
            if (cities[i].port) country_summary_cache[cities[i].owner].ports++;
        }
    }
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            TerrainStats stats;
            if (owner < 0 || owner >= civ_count) continue;
            stats = tile_stats(x, y);
            country_summary_cache[owner].food += stats.food;
            country_summary_cache[owner].livestock += stats.livestock;
            country_summary_cache[owner].wood += stats.wood;
            country_summary_cache[owner].stone += stats.stone;
            country_summary_cache[owner].minerals += stats.minerals;
            country_summary_cache[owner].water += stats.water;
            country_summary_cache[owner].pop_capacity += stats.pop_capacity;
            country_summary_cache[owner].money += stats.money;
            country_summary_cache[owner].habitability += stats.habitability;
        }
    }
    for (i = 0; i < civ_count; i++) {
        CountrySummary *summary = &country_summary_cache[i];
        if (summary->territory <= 0) continue;
        summary->food /= summary->territory;
        summary->livestock /= summary->territory;
        summary->wood /= summary->territory;
        summary->stone /= summary->territory;
        summary->minerals /= summary->territory;
        summary->water /= summary->territory;
        summary->pop_capacity /= summary->territory;
        summary->money /= summary->territory;
        summary->habitability /= summary->territory;
        summary->resource_score = summary->food + summary->livestock + summary->water +
                                  summary->pop_capacity + summary->money + summary->habitability;
    }
    country_summary_dirty = 0;
}

CountrySummary summarize_country(int civ_id) {
    CountrySummary summary;

    memset(&summary, 0, sizeof(summary));
    if (civ_id < 0 || civ_id >= civ_count) return summary;
    if (country_summary_dirty) rebuild_country_summary_cache();
    return country_summary_cache[civ_id];
}

int add_civilization_at(const char *name, char symbol, int aggression, int expansion,
                        int defense, int culture, int preferred_x, int preferred_y) {
    int x = preferred_x;
    int y = preferred_y;
    int city_id;
    Civilization *civ;

    if (civ_count >= MAX_CIVS) return 0;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H || !is_land(world[y][x].geography) ||
        world[y][x].owner != -1 || world[y][x].province_id != -1 || city_too_close(x, y, CITY_MIN_DISTANCE)) {
        if (!find_empty_land(&x, &y)) return 0;
    }

    civ = &civs[civ_count];
    memset(civ, 0, sizeof(*civ));
    strncpy(civ->name, name, NAME_LEN - 1);
    civ->symbol = symbol;
    civ->color = CIV_COLORS[civ_count % MAX_CIVS];
    civ->alive = 1;
    civ->population = 120 + tile_stats(x, y).food * 10 + rnd(80);
    civ->aggression = clamp(aggression, 0, 10);
    civ->expansion = clamp(expansion, 0, 10);
    civ->defense = clamp(defense, 0, 10);
    civ->culture = clamp(culture, 0, 10);
    {
        TerrainStats birth = tile_stats(x, y);
        civ->governance = clamp((defense + culture) / 2 + birth.pop_capacity / 4 + birth.water / 6, 0, 10);
        civ->cohesion = clamp(culture + birth.habitability / 5, 0, 10);
        civ->production = clamp((expansion + defense) / 2 + (birth.wood + birth.stone + birth.minerals) / 8, 0, 10);
        civ->military = clamp(aggression + (birth.minerals + birth.livestock) / 8 + birth.attack / 2, 0, 10);
        civ->commerce = clamp((culture + expansion) / 2 + (birth.money + birth.water) / 5, 0, 10);
        civ->logistics = clamp(expansion + (birth.water + birth.livestock) / 7, 0, 10);
        civ->innovation = clamp((culture + defense) / 2 + (birth.minerals + birth.money) / 8, 0, 10);
        civ->adaptation = clamp((culture + expansion + birth.habitability + clamp(6 - birth.habitability, 0, 6)) / 4, 0, 10);
    }
    civ->capital_city = -1;

    world[y][x].owner = civ_count;
    city_id = create_city(civ_count, x, y, civ->population / 2, 1);
    civ->capital_city = city_id;
    if (city_id >= 0) claim_city_region(city_id, civ_count);

    civ_count++;
    recalculate_territory();
    return 1;
}

static void seed_default_civilizations(void) {
    const char *names[MAX_CIVS] = {
        "Red Dominion", "Blue League", "Green Pact", "Gold Union",
        "Violet Realm", "Orange Clans", "Teal Coast", "Rose March",
        "Ivory Compact", "Black Banner", "Amber League", "Azure Domain",
        "Crimson Isles", "Silver Order", "Jade Senate", "Copper March"
    };
    const char symbols[MAX_CIVS] = {
        'R', 'B', 'G', 'Y', 'V', 'O', 'T', 'M',
        'I', 'K', 'A', 'Z', 'C', 'S', 'J', 'P'
    };
    const int traits[MAX_CIVS][4] = {
        {8, 7, 4, 3}, {3, 4, 8, 6}, {5, 6, 5, 7}, {4, 6, 5, 8},
        {6, 5, 6, 6}, {7, 5, 5, 4}, {4, 7, 4, 7}, {5, 5, 7, 5},
        {2, 6, 8, 8}, {9, 5, 5, 3}, {5, 8, 4, 6}, {3, 7, 6, 7},
        {8, 6, 4, 5}, {4, 4, 9, 8}, {3, 6, 5, 9}, {6, 7, 6, 4}
    };
    int i;
    int count = clamp(initial_civ_count, 0, MAX_CIVS);

    for (i = 0; i < count; i++) {
        add_civilization_at(names[i], symbols[i], traits[i][0], traits[i][1], traits[i][2], traits[i][3], -1, -1);
    }
}

static void update_city_growth_and_regions(void) {
    int i;

    for (i = 0; i < city_count; i++) {
        City *city = &cities[i];
        TerrainStats stats;
        if (!city->alive || city->owner < 0 || city->owner >= civ_count || !civs[city->owner].alive) continue;
        stats = tile_stats(city->x, city->y);
        city->population = clamp(city->population + stats.food + stats.livestock / 2 + stats.water / 2 +
                                 stats.pop_capacity + stats.habitability - 10, 1, MAX_POPULATION);
    }
    region_summary_dirty = 1;
}

static void random_event(char *log, size_t log_size) {
    int id;
    Civilization *civ;

    if (civ_count == 0 || rnd(100) > 18) return;
    id = rnd(civ_count);
    civ = &civs[id];
    if (!civ->alive) return;

    switch (rnd(5)) {
        case 0:
            civ->population += 15 + civ->culture * 3;
            append_log(log, log_size, "Festival in %s. ", civ->name);
            break;
        case 1:
            civ->population = clamp(civ->population - (8 + rnd(28)), 0, MAX_POPULATION);
            civ->disorder_plague = clamp(civ->disorder_plague + 2, 0, 10);
            civ->disorder = clamp(civ->disorder + 2, 0, 10);
            append_log(log, log_size, "Plague hit %s. ", civ->name);
            break;
        case 2:
            civ->defense = clamp(civ->defense + 1, 0, 10);
            civ->disorder_stability = clamp(civ->disorder_stability + 1, 0, 10);
            civ->disorder = clamp(civ->disorder - 1, 0, 10);
            append_log(log, log_size, "%s fortified borders. ", civ->name);
            break;
        case 3:
            civ->aggression = clamp(civ->aggression + 1, 0, 10);
            append_log(log, log_size, "%s became ambitious. ", civ->name);
            break;
        default:
            civ->expansion = clamp(civ->expansion + 1, 0, 10);
            civ->disorder_migration = clamp(civ->disorder_migration + 1, 0, 10);
            civ->disorder = clamp(civ->disorder + 1, 0, 10);
            append_log(log, log_size, "%s started migrating. ", civ->name);
            break;
    }
}

static int living_civilizations(void) {
    int i;
    int living = 0;
    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive) living++;
    }
    return living;
}

static void compute_owned_resource_scores(int scores[MAX_CIVS]) {
    int totals[MAX_CIVS] = {0};
    int counts[MAX_CIVS] = {0};
    int i;
    int x;
    int y;

    for (i = 0; i < MAX_CIVS; i++) scores[i] = 0;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (owner >= 0 && owner < civ_count && civs[owner].alive) {
                TerrainStats stats = tile_stats(x, y);
                totals[owner] += stats.food + stats.livestock + stats.wood + stats.stone +
                                 stats.minerals + stats.water + stats.pop_capacity + stats.money +
                                 stats.habitability;
                counts[owner]++;
            }
        }
    }
    for (i = 0; i < civ_count; i++) {
        scores[i] = counts[i] > 0 ? totals[i] / (counts[i] * 2) : 0;
    }
}

static int compute_dynamic_adaptation(int civ_id, int resource_score) {
    CountrySummary summary = summarize_country(civ_id);
    Civilization *civ = &civs[civ_id];
    int resource_diversity = 0;
    int hardship;
    int social_capacity;
    int instability;
    int score;

    if (summary.food >= 5) resource_diversity++;
    if (summary.livestock >= 5) resource_diversity++;
    if (summary.wood >= 5) resource_diversity++;
    if (summary.stone >= 5) resource_diversity++;
    if (summary.minerals >= 5) resource_diversity++;
    if (summary.water >= 5) resource_diversity++;
    if (summary.money >= 5) resource_diversity++;

    hardship = clamp(10 - summary.habitability, 0, 10) +
               clamp(5 - summary.food, 0, 5) +
               clamp(5 - summary.water, 0, 5);
    social_capacity = civ->culture + civ->cohesion + civ->logistics + civ->innovation;
    instability = civ->disorder + civ->disorder_plague / 2 + civ->disorder_migration / 2;
    score = social_capacity / 4 + hardship / 3 + resource_diversity / 2 +
            clamp(18 - resource_score, 0, 10) / 3 - instability / 2;
    return clamp(score, 0, 10);
}

void simulate_one_month(void) {
    int i;
    int resource_scores[MAX_CIVS];
    char log[512];

    log[0] = '\0';
    compute_owned_resource_scores(resource_scores);
    for (i = 0; i < civ_count; i++) {
        Civilization *civ = &civs[i];
        int resources;
        int growth;
        if (!civ->alive) continue;
        resources = resource_scores[i];
        growth = resources / 2 + civ->governance / 2 + civ->cohesion / 2 + civ->logistics / 3 +
                 civ->territory / 35 + rnd(6) - 4;
        civ->population = clamp(civ->population + growth, 0, MAX_POPULATION);
        civ->disorder_resource = clamp(10 - resources, 0, 10);
        civ->disorder_plague = clamp(civ->disorder_plague - 1, 0, 10);
        civ->disorder_migration = clamp(civ->disorder_migration - 1, 0, 10);
        civ->disorder_stability = clamp(civ->disorder_stability - 1, 0, 10);
        civ->disorder = clamp(civ->disorder + (resources < 10 ? 1 : -1) - civ->governance / 8 - civ->cohesion / 8, 0, 10);
        civ->adaptation = compute_dynamic_adaptation(i, resources);
    }

    update_city_growth_and_regions();
    ports_update_migration();
    country_summary_dirty = 1;
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        expansion_update_civilization(i, resource_scores[i], log, sizeof(log));
    }

    random_event(log, sizeof(log));
    recalculate_territory();
    diplomacy_update_contacts();
    month++;
    if (month > 12) {
        month = 1;
        year = clamp(year + 1, 0, 99999);
        diplomacy_update_year();
        war_update_year();
    }
    if (living_civilizations() <= 1) auto_run = 0;
}

void reset_simulation(void) {
    diplomacy_reset();
    war_reset();
    year = 0;
    month = 1;
    civ_count = 0;
    city_count = 0;
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    invalidate_region_cache();
    generate_world();
    invalidate_region_cache();
    seed_default_civilizations();
    recalculate_territory();
    diplomacy_update_contacts();
    auto_run = 0;
}
