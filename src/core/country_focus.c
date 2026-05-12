#include "core/country_focus.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <string.h>

typedef struct {
    int valid;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int focus_x;
    int focus_y;
    int has_capital;
} CountryFocusCache;

static CountryFocusCache focus_cache[MAX_CIVS];
static int focus_revision = 0;
static int focus_map_w = 0;
static int focus_map_h = 0;

void country_focus_invalidate(void) { focus_revision = 0; }

static void reset_cache(void) {
    int i;
    for (i = 0; i < MAX_CIVS; i++) {
        focus_cache[i].valid = 0;
        focus_cache[i].min_x = MAP_W;
        focus_cache[i].min_y = MAP_H;
        focus_cache[i].max_x = -1;
        focus_cache[i].max_y = -1;
        focus_cache[i].focus_x = 0;
        focus_cache[i].focus_y = 0;
        focus_cache[i].has_capital = 0;
    }
}

static void add_tile(int civ_id, int x, int y) {
    CountryFocusCache *entry;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    entry = &focus_cache[civ_id];
    entry->valid = 1;
    if (x < entry->min_x) entry->min_x = x;
    if (y < entry->min_y) entry->min_y = y;
    if (x > entry->max_x) entry->max_x = x;
    if (y > entry->max_y) entry->max_y = y;
}

static int valid_focus_tile_for_civ(int civ_id, int x, int y) {
    int region_id;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    if (!is_land(world[y][x].geography)) return 0;
    if (world[y][x].owner == civ_id) return 1;
    region_id = world[y][x].region_id;
    return region_id >= 0 && region_id < region_count && natural_regions[region_id].owner_civ == civ_id;
}

static int valid_capital_city_for_civ(int civ_id, int city_id) {
    if (city_id < 0 || city_id >= city_count) return 0;
    if (!cities[city_id].alive || cities[city_id].owner != civ_id) return 0;
    return valid_focus_tile_for_civ(civ_id, cities[city_id].x, cities[city_id].y);
}

static int region_component_center(int civ_id, int start_region, int *out_x, int *out_y, int *out_tiles) {
    int queue[MAX_NATURAL_REGIONS];
    static unsigned char seen[MAX_NATURAL_REGIONS];
    int head = 0;
    int tail = 0;
    int sum_x = 0;
    int sum_y = 0;
    int tiles = 0;
    int i;

    if (start_region < 0 || start_region >= region_count ||
        natural_regions[start_region].owner_civ != civ_id) return 0;
    memset(seen, 0, sizeof(seen));
    seen[start_region] = 1;
    queue[tail++] = start_region;
    while (head < tail) {
        NaturalRegion *region = &natural_regions[queue[head++]];
        int weight = max(1, region->tile_count);
        sum_x += region->center_x * weight;
        sum_y += region->center_y * weight;
        tiles += weight;
        for (i = 0; i < region->neighbor_count; i++) {
            int neighbor = region->neighbors[i];
            if (neighbor < 0 || neighbor >= region_count || seen[neighbor]) continue;
            if (natural_regions[neighbor].owner_civ != civ_id) continue;
            seen[neighbor] = 1;
            queue[tail++] = neighbor;
        }
    }
    if (tiles <= 0) return 0;
    if (out_x) *out_x = clamp(sum_x / tiles, 0, MAP_W - 1);
    if (out_y) *out_y = clamp(sum_y / tiles, 0, MAP_H - 1);
    if (out_tiles) *out_tiles = tiles;
    return 1;
}

static int largest_owned_component_center(int civ_id, int *out_x, int *out_y) {
    static unsigned char global_seen[MAX_NATURAL_REGIONS];
    int best_tiles = -1;
    int best_x = 0;
    int best_y = 0;
    int i;

    memset(global_seen, 0, sizeof(global_seen));
    for (i = 0; i < region_count; i++) {
        int queue[MAX_NATURAL_REGIONS];
        int head = 0;
        int tail = 0;
        int sum_x = 0;
        int sum_y = 0;
        int tiles = 0;
        if (global_seen[i] || natural_regions[i].owner_civ != civ_id) continue;
        global_seen[i] = 1;
        queue[tail++] = i;
        while (head < tail) {
            NaturalRegion *region = &natural_regions[queue[head++]];
            int n;
            int weight = max(1, region->tile_count);
            sum_x += region->center_x * weight;
            sum_y += region->center_y * weight;
            tiles += weight;
            for (n = 0; n < region->neighbor_count; n++) {
                int neighbor = region->neighbors[n];
                if (neighbor < 0 || neighbor >= region_count || global_seen[neighbor]) continue;
                if (natural_regions[neighbor].owner_civ != civ_id) continue;
                global_seen[neighbor] = 1;
                queue[tail++] = neighbor;
            }
        }
        if (tiles > best_tiles) {
            best_tiles = tiles;
            best_x = sum_x / max(1, tiles);
            best_y = sum_y / max(1, tiles);
        }
    }
    if (best_tiles <= 0) return 0;
    if (out_x) *out_x = clamp(best_x, 0, MAP_W - 1);
    if (out_y) *out_y = clamp(best_y, 0, MAP_H - 1);
    return 1;
}

static int nearest_owned_land_tile(int civ_id, int target_x, int target_y, int *out_x, int *out_y) {
    int x;
    int y;
    int best_x = -1;
    int best_y = -1;
    int best_d = 0x7fffffff;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int dx;
            int dy;
            int d;
            if (world[y][x].owner != civ_id || !is_land(world[y][x].geography)) continue;
            dx = x - target_x;
            dy = y - target_y;
            d = dx * dx + dy * dy;
            if (d < best_d) {
                best_d = d;
                best_x = x;
                best_y = y;
            }
        }
    }
    if (best_x < 0) return 0;
    if (out_x) *out_x = best_x;
    if (out_y) *out_y = best_y;
    return 1;
}

static int set_focus_near_owned_land(CountryFocusCache *entry, int civ_id, int x, int y) {
    int land_x;
    int land_y;
    if (!nearest_owned_land_tile(civ_id, x, y, &land_x, &land_y)) return 0;
    entry->focus_x = land_x;
    entry->focus_y = land_y;
    return 1;
}

static void choose_focus_for_civ(int civ_id) {
    CountryFocusCache *entry = &focus_cache[civ_id];
    int city_id = civs[civ_id].capital_city;
    int region_id;

    if (!entry->valid) return;
    if (valid_capital_city_for_civ(civ_id, city_id)) {
        entry->focus_x = cities[city_id].x;
        entry->focus_y = cities[city_id].y;
        entry->has_capital = 1;
        return;
    }
    if (city_id >= 0 && city_id < city_count &&
        cities[city_id].x >= 0 && cities[city_id].x < MAP_W &&
        cities[city_id].y >= 0 && cities[city_id].y < MAP_H) {
        int cx;
        int cy;
        region_id = world[cities[city_id].y][cities[city_id].x].region_id;
        if (region_component_center(civ_id, region_id, &cx, &cy, NULL) &&
            set_focus_near_owned_land(entry, civ_id, cx, cy)) return;
    }
    {
        int cx;
        int cy;
        if (largest_owned_component_center(civ_id, &cx, &cy) &&
            set_focus_near_owned_land(entry, civ_id, cx, cy)) return;
    }
    set_focus_near_owned_land(entry, civ_id, (entry->min_x + entry->max_x) / 2,
                              (entry->min_y + entry->max_y) / 2);
}

static void rebuild_cache(void) {
    int x;
    int y;
    int i;

    reset_cache();
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            add_tile(world[y][x].owner, x, y);
        }
    }
    for (i = 0; i < MAX_CIVS; i++) {
        choose_focus_for_civ(i);
    }
    focus_revision = dirty_revision_ownership();
    focus_map_w = MAP_W;
    focus_map_h = MAP_H;
}

static void ensure_cache(void) {
    if (focus_revision != dirty_revision_ownership() || focus_map_w != MAP_W || focus_map_h != MAP_H) {
        rebuild_cache();
    }
}

int country_bounds(int civ_id, int *min_x, int *min_y, int *max_x, int *max_y) {
    CountryFocusCache *entry;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    ensure_cache();
    entry = &focus_cache[civ_id];
    if (!entry->valid || entry->max_x < entry->min_x || entry->max_y < entry->min_y) return 0;
    if (min_x) *min_x = entry->min_x;
    if (min_y) *min_y = entry->min_y;
    if (max_x) *max_x = entry->max_x;
    if (max_y) *max_y = entry->max_y;
    return 1;
}

int country_focus_point(int civ_id, int *x, int *y) {
    CountryFocusCache *entry;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    ensure_cache();
    entry = &focus_cache[civ_id];
    if (!entry->valid) return 0;
    if (x) *x = entry->focus_x;
    if (y) *y = entry->focus_y;
    return 1;
}
