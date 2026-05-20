#include "sim/diplomacy_borders.h"

#include "core/game_types.h"
#include "core/profiler.h"
#include "sim/diplomacy.h"
#include "world/terrain_query.h"

#include <string.h>

static BorderContactCache border_contact_cache;

static int is_natural_barrier_tile(int x, int y) {
    Geography geography;
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 0;
    geography = world[y][x].geography;
    return world[y][x].river || geography == GEO_MOUNTAIN || geography == GEO_CANYON ||
           geography == GEO_COAST || world_water_depth_at(x, y) != WATER_DEPTH_NONE;
}

static void rebuild_border_contact_cache(void) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int x, y;
    memset(&border_contact_cache, 0, sizeof(border_contact_cache));
    profiler_add_scanned_tiles(MAP_W * MAP_H);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            int d;
            if (owner < 0 || owner >= civ_count || world[y][x].province_id < 0 ||
                !civs[owner].alive) continue;
            for (d = 0; d < 4; d++) {
                int nx = x + dirs[d][0];
                int ny = y + dirs[d][1];
                int other;
                if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
                other = world[ny][nx].owner;
                if (other < 0 || other >= civ_count || other == owner ||
                    world[ny][nx].province_id < 0 || !civs[other].alive) continue;
                border_contact_cache.border_length[owner][other]++;
                if (is_natural_barrier_tile(x, y) || is_natural_barrier_tile(nx, ny)) {
                    border_contact_cache.natural_barrier[owner][other]++;
                }
            }
        }
    }
    border_contact_cache.dirty = 0;
}

void diplomacy_borders_reset(void) {
    memset(&border_contact_cache, 0, sizeof(border_contact_cache));
    border_contact_cache.dirty = 1;
}

void diplomacy_borders_mark_dirty(void) { border_contact_cache.dirty = 1; }
int diplomacy_borders_dirty(void) { return border_contact_cache.dirty; }

void diplomacy_borders_ensure(void) {
    if (border_contact_cache.dirty) rebuild_border_contact_cache();
}

void diplomacy_borders_clear_civ(int civ_id) {
    int i;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    for (i = 0; i < MAX_CIVS; i++) {
        border_contact_cache.border_length[civ_id][i] = 0;
        border_contact_cache.border_length[i][civ_id] = 0;
        border_contact_cache.natural_barrier[civ_id][i] = 0;
        border_contact_cache.natural_barrier[i][civ_id] = 0;
    }
    border_contact_cache.dirty = 1;
}

int diplomacy_pair_contact_stats(int civ_a, int civ_b, int *border_length, int *natural_barrier) {
    diplomacy_borders_ensure();
    if (border_length) *border_length = border_contact_cache.border_length[civ_a][civ_b];
    if (natural_barrier) *natural_barrier = border_contact_cache.natural_barrier[civ_a][civ_b];
    return border_contact_cache.border_length[civ_a][civ_b] > 0;
}
