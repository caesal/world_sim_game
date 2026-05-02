#include "world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void invalidate_region_cache(void);

static TerrainStats terrain_stats_base(Geography geography, Climate climate, int river) {
    TerrainStats stats;

    memset(&stats, 0, sizeof(stats));
    switch (geography) {
        case GEO_OCEAN:
            stats.water = 10;
            stats.habitability = 0;
            break;
        case GEO_SHOAL:
            stats.food = 5;
            stats.livestock = 1;
            stats.water = 9;
            stats.habitability = 4;
            stats.defense = 1;
            break;
        case GEO_PLAIN:
            stats.food = 6;
            stats.livestock = 6;
            stats.wood = 2;
            stats.water = 4;
            stats.habitability = 7;
            break;
        case GEO_BASIN:
            stats.food = 5;
            stats.livestock = 4;
            stats.wood = 2;
            stats.minerals = 2;
            stats.water = 6;
            stats.habitability = 6;
            stats.defense = 1;
            break;
        case GEO_HILL:
            stats.food = 3;
            stats.livestock = 3;
            stats.wood = 3;
            stats.minerals = 6;
            stats.water = 3;
            stats.habitability = 4;
            stats.attack = 1;
            stats.defense = 2;
            break;
        case GEO_MOUNTAIN:
            stats.food = 1;
            stats.livestock = 1;
            stats.wood = 1;
            stats.minerals = 9;
            stats.water = 3;
            stats.habitability = 2;
            stats.attack = 2;
            stats.defense = 4;
            break;
    }

    switch (climate) {
        case CLIMATE_GRASSLAND:
            stats.food += 3;
            stats.livestock += 3;
            stats.habitability += 2;
            stats.attack -= 1;
            break;
        case CLIMATE_FOREST:
            stats.wood += 5;
            stats.food += 1;
            stats.livestock -= 1;
            stats.defense += 2;
            break;
        case CLIMATE_WETLAND:
            stats.food += 2;
            stats.livestock += 1;
            stats.water += 3;
            stats.habitability -= 1;
            stats.defense += 1;
            break;
        case CLIMATE_SWAMP:
            stats.food += 1;
            stats.water += 4;
            stats.habitability -= 2;
            stats.defense += 2;
            break;
        case CLIMATE_TUNDRA:
            stats.food -= 2;
            stats.livestock -= 1;
            stats.habitability -= 2;
            stats.attack += 1;
            stats.defense += 1;
            break;
        case CLIMATE_SNOWFIELD:
            stats.food -= 3;
            stats.livestock -= 2;
            stats.habitability -= 4;
            stats.attack += 2;
            stats.defense += 1;
            break;
        case CLIMATE_DESERT:
            stats.food -= 3;
            stats.livestock -= 2;
            stats.water -= 3;
            stats.minerals += 2;
            stats.habitability -= 4;
            stats.attack += 2;
            break;
    }

    if (river) {
        stats.food += 2;
        stats.water += 3;
        stats.defense += 1;
    }

    stats.food = clamp(stats.food, 0, 10);
    stats.livestock = clamp(stats.livestock, 0, 10);
    stats.wood = clamp(stats.wood, 0, 10);
    stats.minerals = clamp(stats.minerals, 0, 10);
    stats.water = clamp(stats.water, 0, 10);
    stats.habitability = clamp(stats.habitability, 0, 10);
    stats.attack = clamp(stats.attack, -3, 6);
    stats.defense = clamp(stats.defense, 0, 8);
    return stats;
}

const char *geography_name(Geography geography) {
    switch (geography) {
        case GEO_OCEAN: return "Ocean";
        case GEO_SHOAL: return "Shoal";
        case GEO_PLAIN: return "Plain";
        case GEO_BASIN: return "Basin";
        case GEO_HILL: return "Hill";
        case GEO_MOUNTAIN: return "Mountain";
        default: return "Unknown";
    }
}

const char *climate_name(Climate climate) {
    switch (climate) {
        case CLIMATE_GRASSLAND: return "Grassland";
        case CLIMATE_FOREST: return "Forest";
        case CLIMATE_WETLAND: return "Wetland";
        case CLIMATE_SWAMP: return "Swamp";
        case CLIMATE_TUNDRA: return "Tundra";
        case CLIMATE_SNOWFIELD: return "Snowfield";
        case CLIMATE_DESERT: return "Desert";
        default: return "Unknown";
    }
}

COLORREF geography_color(Geography geography) {
    switch (geography) {
        case GEO_OCEAN: return RGB(74, 139, 201);
        case GEO_SHOAL: return RGB(200, 227, 241);
        case GEO_PLAIN: return RGB(189, 190, 157);
        case GEO_BASIN: return RGB(165, 166, 135);
        case GEO_HILL: return RGB(140, 125, 88);
        case GEO_MOUNTAIN: return RGB(101, 92, 81);
        default: return RGB(0, 0, 0);
    }
}

COLORREF climate_color(Climate climate) {
    switch (climate) {
        case CLIMATE_GRASSLAND: return RGB(134, 191, 71);
        case CLIMATE_FOREST: return RGB(49, 144, 54);
        case CLIMATE_WETLAND: return RGB(120, 176, 93);
        case CLIMATE_SWAMP: return RGB(74, 126, 78);
        case CLIMATE_TUNDRA: return RGB(192, 196, 151);
        case CLIMATE_SNOWFIELD: return RGB(234, 230, 213);
        case CLIMATE_DESERT: return RGB(227, 185, 78);
        default: return RGB(0, 0, 0);
    }
}

COLORREF overview_color(int x, int y) {
    COLORREF base = geography_color(world[y][x].geography);
    COLORREF climate = climate_color(world[y][x].climate);
    int blend = is_land(world[y][x].geography) ? 48 : 18;
    COLORREF color = blend_color(base, climate, blend);
    int elev = world[y][x].elevation;

    if (!is_land(world[y][x].geography)) return color;
    if (elev > 55) return blend_color(color, RGB(38, 35, 32), clamp((elev - 55) / 3, 0, 18));
    return blend_color(color, RGB(236, 230, 198), clamp((55 - elev) / 4, 0, 12));
}

int is_land(Geography geography) {
    return geography != GEO_OCEAN && geography != GEO_SHOAL;
}

TerrainStats tile_stats(int x, int y) {
    TerrainStats stats = terrain_stats_base(world[y][x].geography, world[y][x].climate, world[y][x].river);
    int variation = world[y][x].resource_variation - 50;

    stats.food = clamp(stats.food + variation / 18, 0, 10);
    stats.livestock = clamp(stats.livestock + variation / 22 + (world[y][x].moisture - 45) / 30, 0, 10);
    stats.wood = clamp(stats.wood + (world[y][x].moisture - 50) / 24, 0, 10);
    stats.minerals = clamp(stats.minerals + (world[y][x].elevation - 55) / 18, 0, 10);
    stats.water = clamp(stats.water + (world[y][x].moisture - 45) / 18, 0, 10);
    stats.habitability = clamp(stats.habitability + variation / 24 - abs(world[y][x].temperature - 55) / 35, 0, 10);
    if (world[y][x].river) stats.defense = clamp(stats.defense + 1, 0, 8);
    return stats;
}

static int nearby_land_count(int x, int y) {
    int count = 0;
    int dy;
    int dx;

    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (dx == 0 && dy == 0) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (is_land(world[ny][nx].geography)) count++;
        }
    }
    return count;
}

static int tile_cost(int x, int y) {
    TerrainStats stats = tile_stats(x, y);
    if (!is_land(world[y][x].geography)) return 99;
    return clamp(10 - stats.habitability + stats.defense / 2, 2, 9);
}

static int province_cache[MAP_H][MAP_W];
static int province_cache_dirty = 1;
static RegionSummary region_summary_cache[MAX_CITIES];
static int region_summary_dirty = 1;

static void invalidate_region_cache(void) {
    province_cache_dirty = 1;
    region_summary_dirty = 1;
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
            region_summary_cache[city_id].minerals += stats.minerals;
            region_summary_cache[city_id].water += stats.water;
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
        summary->minerals /= summary->tiles;
        summary->water /= summary->tiles;
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

static void seed_random(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    srand((unsigned int)(time(NULL) ^ GetTickCount() ^ counter.LowPart ^ GetCurrentProcessId()));
}

static int compare_ints(const void *left, const void *right) {
    int a = *(const int *)left;
    int b = *(const int *)right;
    return (a > b) - (a < b);
}

static void smooth_field(int field[MAP_H][MAP_W], int passes) {
    int pass;

    for (pass = 0; pass < passes; pass++) {
        static int next[MAP_H][MAP_W];
        int y;
        int x;

        for (y = 0; y < MAP_H; y++) {
            for (x = 0; x < MAP_W; x++) {
                int total = 0;
                int count = 0;
                int dy;
                int dx;

                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                        total += field[ny][nx];
                        count++;
                    }
                }
                next[y][x] = total / count;
            }
        }
        memcpy(field, next, sizeof(next));
    }
}

static Climate majority_neighbor_climate(int x, int y, Climate fallback) {
    int counts[7] = {0};
    int best = fallback;
    int best_count = -1;
    int dy;
    int dx;

    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            Climate climate;
            if (dx == 0 && dy == 0) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) continue;
            climate = world[ny][nx].climate;
            counts[climate]++;
        }
    }

    for (dx = 0; dx < 7; dx++) {
        if (counts[dx] > best_count) {
            best = (Climate)dx;
            best_count = counts[dx];
        }
    }
    return best;
}

static void mark_river_from(int start_x, int start_y) {
    int x = start_x;
    int y = start_y;
    int steps;

    for (steps = 0; steps < 180; steps++) {
        int best_x = x;
        int best_y = y;
        int best_score = world[y][x].elevation;
        int dy;
        int dx;

        if (!is_land(world[y][x].geography)) return;
        world[y][x].river = 1;

        for (dy = -1; dy <= 1; dy++) {
            for (dx = -1; dx <= 1; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                int score;

                if (dx == 0 && dy == 0) continue;
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                if (world[ny][nx].geography == GEO_OCEAN || world[ny][nx].geography == GEO_SHOAL) return;
                score = world[ny][nx].elevation + rnd(5);
                if (score < best_score) {
                    best_score = score;
                    best_x = nx;
                    best_y = ny;
                }
            }
        }

        if (best_x == x && best_y == y) return;
        x = best_x;
        y = best_y;
    }
}

static Geography classify_geography(int elevation, int sea_level, int nearby_water) {
    int shoal_band = 1 + ocean_slider * 5 / 100;
    int shoal_water_limit = ocean_slider < 15 ? 7 : 4;

    if (elevation < sea_level) return GEO_OCEAN;
    if (elevation < sea_level + shoal_band || nearby_water >= shoal_water_limit) return GEO_SHOAL;
    if (elevation > 80) return GEO_MOUNTAIN;
    if (elevation > 66) return GEO_HILL;
    if (elevation < sea_level + 12) return GEO_BASIN;
    return GEO_PLAIN;
}

static Climate classify_climate(int cold, int elevation, int moisture, Geography geography) {
    int desert_limit = 12 + desert_slider * 28 / 100;
    int forest_limit = 70 - forest_slider * 35 / 100;
    int wetland_limit = 92 - wetland_slider * 30 / 100;
    int tundra_limit = 80 - mountain_slider / 10;
    int snow_limit = 96 - mountain_slider / 8;

    if (!is_land(geography)) return CLIMATE_GRASSLAND;
    if (cold > snow_limit) return CLIMATE_SNOWFIELD;
    if (cold > tundra_limit) return CLIMATE_TUNDRA;
    if (moisture < desert_limit && cold < 65) return CLIMATE_DESERT;
    if (moisture > wetland_limit && geography == GEO_BASIN) return CLIMATE_SWAMP;
    if (moisture > wetland_limit - 10 && elevation < 58) return CLIMATE_WETLAND;
    if (moisture > forest_limit) return CLIMATE_FOREST;
    return CLIMATE_GRASSLAND;
}

static void generate_world(void) {
    int y;
    int x;
    int pass;
    static int elevation[MAP_H][MAP_W];
    static int moisture[MAP_H][MAP_W];
    static int temperature[MAP_H][MAP_W];
    static int sorted_elevation[MAP_W * MAP_H];
    int target_land_percent = 96 - ocean_slider * 76 / 100;
    int target_land_tiles = MAP_W * MAP_H * target_land_percent / 100;
    int threshold_index = clamp(MAP_W * MAP_H - target_land_tiles, 0, MAP_W * MAP_H - 1);
    int sea_level;
    int index = 0;
    int cold_x1 = rnd(MAP_W);
    int cold_y1 = rnd(MAP_H);
    int cold_x2 = rnd(MAP_W);
    int cold_y2 = rnd(MAP_H);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int edge_x = x < MAP_W / 2 ? x : MAP_W - 1 - x;
            int edge_y = y < MAP_H / 2 ? y : MAP_H - 1 - y;
            int edge = edge_x < edge_y ? edge_x : edge_y;
            int edge_falloff = clamp(edge * 3, 0, 42);

            world[y][x].geography = GEO_OCEAN;
            world[y][x].climate = CLIMATE_GRASSLAND;
            world[y][x].owner = -1;
            world[y][x].province_id = -1;
            world[y][x].river = 0;
            elevation[y][x] = rnd(101) + edge_falloff - 22;
            moisture[y][x] = rnd(101);
            temperature[y][x] = rnd(101);
        }
    }

    smooth_field(elevation, 11);
    smooth_field(moisture, 9);
    smooth_field(temperature, 7);

    for (pass = 0; pass < 32; pass++) {
        int cx = 24 + rnd(MAP_W - 48);
        int cy = 18 + rnd(MAP_H - 36);
        int radius = 18 + rnd(62);
        int lift = 8 + mountain_slider * 28 / 100 + rnd(22);
        int yy;
        int xx;

        for (yy = cy - radius; yy <= cy + radius; yy++) {
            for (xx = cx - radius; xx <= cx + radius; xx++) {
                int dx = xx - cx;
                int dy = (yy - cy) * 2;
                int dist = abs(dx) + abs(dy);
                if (xx < 0 || xx >= MAP_W || yy < 0 || yy >= MAP_H || dist > radius * 2) continue;
                elevation[yy][xx] += lift * (radius * 2 - dist) / (radius * 2);
            }
        }
    }
    smooth_field(elevation, 5);

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) sorted_elevation[index++] = elevation[y][x];
    }
    qsort(sorted_elevation, MAP_W * MAP_H, sizeof(sorted_elevation[0]), compare_ints);
    sea_level = sorted_elevation[threshold_index];

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int elev = clamp(elevation[y][x], 0, 100);
            int moist = clamp(moisture[y][x] + (elev < sea_level + 8 ? 12 : 0), 0, 100);
            int cold_dist_1 = abs(x - cold_x1) * 100 / MAP_W + abs(y - cold_y1) * 100 / MAP_H;
            int cold_dist_2 = abs(x - cold_x2) * 100 / MAP_W + abs(y - cold_y2) * 100 / MAP_H;
            int cold = clamp(95 - (cold_dist_1 < cold_dist_2 ? cold_dist_1 : cold_dist_2) + elev / 5 + rnd(21) - 10, 0, 120);
            int temp = clamp(100 - cold + temperature[y][x] / 8, 0, 100);
            int nearby_water = 0;
            int dy;
            int dx;

            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                    if (elevation[ny][nx] < sea_level) nearby_water++;
                }
            }

            world[y][x].elevation = elev;
            world[y][x].moisture = moist;
            world[y][x].temperature = temp;
            world[y][x].resource_variation = rnd(101);
            world[y][x].geography = classify_geography(elev, sea_level, nearby_water);
            world[y][x].climate = classify_climate(cold, elev, moist, world[y][x].geography);
        }
    }

    for (pass = 0; pass < (ocean_slider < 15 ? 5 : 2); pass++) {
        static Tile next[MAP_H][MAP_W];
        memcpy(next, world, sizeof(world));
        for (y = 1; y < MAP_H - 1; y++) {
            for (x = 1; x < MAP_W - 1; x++) {
                int land = nearby_land_count(x, y);
                if (is_land(world[y][x].geography) && land <= 2) next[y][x].geography = GEO_OCEAN;
                if (!is_land(world[y][x].geography) && land >= (ocean_slider < 15 ? 4 : 6)) next[y][x].geography = GEO_PLAIN;
            }
        }
        memcpy(world, next, sizeof(world));
    }

    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            if (is_land(world[y][x].geography)) {
                world[y][x].climate = majority_neighbor_climate(x, y, world[y][x].climate);
            } else {
                world[y][x].climate = CLIMATE_GRASSLAND;
            }
        }
    }

    for (pass = 0; pass < 52; pass++) {
        int tries;
        for (tries = 0; tries < 250; tries++) {
            int rx = rnd(MAP_W);
            int ry = rnd(MAP_H);
            if ((world[ry][rx].geography == GEO_MOUNTAIN || world[ry][rx].geography == GEO_HILL) &&
                world[ry][rx].moisture > 35 + rnd(30)) {
                mark_river_from(rx, ry);
                break;
            }
        }
    }

    year = 0;
    month = 1;
    civ_count = 0;
    city_count = 0;
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    invalidate_region_cache();
}

static int city_too_close(int x, int y, int min_distance);

static int find_empty_land(int *out_x, int *out_y) {
    int tries;

    for (tries = 0; tries < 12000; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        TerrainStats stats = tile_stats(x, y);
        if (is_land(world[y][x].geography) && world[y][x].owner == -1 && world[y][x].province_id == -1 &&
            stats.habitability > 1 && !city_too_close(x, y, CITY_MIN_DISTANCE)) {
            *out_x = x;
            *out_y = y;
            return 1;
        }
    }
    return 0;
}

static int city_too_close(int x, int y, int min_distance) {
    int i;
    int min_distance_sq = min_distance * min_distance;

    for (i = 0; i < city_count; i++) {
        int dx;
        int dy;
        if (!cities[i].alive) continue;
        dx = x - cities[i].x;
        dy = y - cities[i].y;
        if (dx * dx + dy * dy < min_distance_sq) return 1;
    }
    return 0;
}

static int city_radius_for_tile(int x, int y, int population) {
    TerrainStats stats = tile_stats(x, y);
    int resource_score = stats.food + stats.livestock + stats.water + stats.habitability + stats.wood / 2 + stats.minerals / 2;
    return clamp(2 + population / 180 + resource_score / 8, 3, 10);
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
    city->population = population;
    city->radius = city_radius_for_tile(x, y, population);
    city->capital = capital;
    return city_count++;
}

static void claim_city_region(int city_id, int owner) {
    int x;
    int y;
    int has_existing_region = 0;
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

    for (y = city->y - city->radius; y <= city->y + city->radius; y++) {
        for (x = city->x - city->radius; x <= city->x + city->radius; x++) {
            int dx = x - city->x;
            int dy = y - city->y;
            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            if (!is_land(world[y][x].geography)) continue;
            if (world[y][x].province_id >= 0 && world[y][x].province_id != city_id) continue;
            if (world[y][x].owner >= 0 && world[y][x].owner != owner) continue;
            if (dx * dx + dy * dy <= city->radius * city->radius) {
                world[y][x].owner = owner;
                world[y][x].province_id = city_id;
            }
        }
    }
    invalidate_region_cache();
}

int city_at(int x, int y) {
    int i;

    for (i = 0; i < city_count; i++) {
        if (cities[i].alive && cities[i].x == x && cities[i].y == y) return i;
    }
    return -1;
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
        "Violet Realm", "Orange Clans", "Teal Coast", "Rose March"
    };
    const char symbols[MAX_CIVS] = {'R', 'B', 'G', 'Y', 'V', 'O', 'T', 'M'};
    const int traits[MAX_CIVS][4] = {
        {8, 7, 4, 3}, {3, 4, 8, 6}, {5, 6, 5, 7}, {4, 6, 5, 8},
        {6, 5, 6, 6}, {7, 5, 5, 4}, {4, 7, 4, 7}, {5, 5, 7, 5}
    };
    int i;
    int count = clamp(initial_civ_count, 0, MAX_CIVS);

    for (i = 0; i < count; i++) {
        add_civilization_at(names[i], symbols[i], traits[i][0], traits[i][1], traits[i][2], traits[i][3], -1, -1);
    }
}

static int pick_owned_border(int civ_id, int *to_x, int *to_y) {
    int tries;
    const int dir[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (tries = 0; tries < 6000; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        int d = rnd(4);
        int nx = x + dir[d][0];
        int ny = y + dir[d][1];

        if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
        if (world[y][x].owner != civ_id) continue;
        if (!is_land(world[ny][nx].geography)) continue;
        if (world[ny][nx].owner != -1) continue;
        *to_x = nx;
        *to_y = ny;
        return 1;
    }
    return 0;
}

static void maybe_found_city(int civ_id) {
    int tries;

    if (city_count >= MAX_CITIES || !civs[civ_id].alive) return;
    if (civs[civ_id].territory < 16 || rnd(100) > 5 + civs[civ_id].culture) return;

    for (tries = 0; tries < 2500; tries++) {
        int x = rnd(MAP_W);
        int y = rnd(MAP_H);
        TerrainStats stats = tile_stats(x, y);
        if (world[y][x].owner != civ_id) continue;
        if (!is_land(world[y][x].geography)) continue;
        if (world[y][x].province_id != -1) continue;
        if (city_at(x, y) >= 0 || city_too_close(x, y, CITY_MIN_DISTANCE)) continue;
        if (stats.food + stats.water + stats.habitability < 14) continue;
        {
            int city_id = create_city(civ_id, x, y, 80 + stats.food * 8 + rnd(80), 0);
            if (city_id >= 0) claim_city_region(city_id, civ_id);
        }
        return;
    }
}

static void resolve_expansion(int civ_id, char *log, size_t log_size) {
    int tx;
    int ty;
    Civilization *civ = &civs[civ_id];
    TerrainStats target_stats;
    int drive;
    int cost;

    if (!civ->alive) return;
    if (!pick_owned_border(civ_id, &tx, &ty)) return;

    target_stats = tile_stats(tx, ty);
    cost = tile_cost(tx, ty);
    drive = civ->expansion * 8 + civ->culture * 2 + target_stats.habitability * 2 +
            rnd(45) + civ->population / 70 - cost * 5;

    if (drive > 22) {
        world[ty][tx].owner = civ_id;
        append_log(log, log_size, "%s settled new land. ", civ->name);
    }
}

static void update_city_growth_and_regions(void) {
    int i;

    for (i = 0; i < city_count; i++) {
        City *city = &cities[i];
        TerrainStats stats;
        if (!city->alive || city->owner < 0 || city->owner >= civ_count || !civs[city->owner].alive) continue;
        stats = tile_stats(city->x, city->y);
        city->population = clamp(city->population + stats.food + stats.livestock / 2 + stats.water / 2 + stats.habitability - 8, 1, 999999);
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
            civ->population = clamp(civ->population - (8 + rnd(28)), 0, 999999);
            civ->disorder = clamp(civ->disorder + 2, 0, 10);
            append_log(log, log_size, "Plague hit %s. ", civ->name);
            break;
        case 2:
            civ->defense = clamp(civ->defense + 1, 0, 10);
            civ->disorder = clamp(civ->disorder - 1, 0, 10);
            append_log(log, log_size, "%s fortified borders. ", civ->name);
            break;
        case 3:
            civ->aggression = clamp(civ->aggression + 1, 0, 10);
            append_log(log, log_size, "%s became ambitious. ", civ->name);
            break;
        default:
            civ->expansion = clamp(civ->expansion + 1, 0, 10);
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
                totals[owner] += stats.food + stats.livestock + stats.water + stats.habitability;
                counts[owner]++;
            }
        }
    }
    for (i = 0; i < civ_count; i++) {
        scores[i] = counts[i] > 0 ? totals[i] / counts[i] : 0;
    }
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
        growth = resources / 2 + civ->culture + civ->territory / 30 + rnd(6) - 4;
        civ->population = clamp(civ->population + growth, 0, 999999);
        civ->disorder = clamp(civ->disorder + (resources < 10 ? 1 : -1), 0, 10);
    }

    update_city_growth_and_regions();
    for (i = 0; i < civ_count; i++) {
        int attempts;
        if (!civs[i].alive) continue;
        attempts = 1 + civs[i].expansion / 4 + civs[i].culture / 8;
        while (attempts-- > 0) resolve_expansion(i, log, sizeof(log));
        maybe_found_city(i);
    }

    random_event(log, sizeof(log));
    recalculate_territory();
    month++;
    if (month > 12) {
        month = 1;
        year = clamp(year + 1, 0, 99999);
    }
    if (living_civilizations() <= 1) auto_run = 0;
}

void reset_simulation(void) {
    seed_random();
    generate_world();
    seed_default_civilizations();
    recalculate_territory();
    auto_run = 0;
}
