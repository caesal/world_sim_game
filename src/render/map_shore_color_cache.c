#include "render/map_shore_color_cache.h"

#include "world/terrain_query.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int *donor_indices;
static int donor_capacity;
static int source_map_w;
static int source_map_h;
static int source_terrain_revision;
static int source_coast_revision;
static int source_valid;
static MapShoreColorCacheStats stats;

static int donor_geography(int geography) {
    return geography != GEO_COAST && geography != GEO_ISLAND &&
           geography != GEO_WETLAND && geography != GEO_DELTA;
}

static int source_matches(const RenderSnapshot *snapshot) {
    return source_valid && snapshot &&
           source_map_w == snapshot->map_w &&
           source_map_h == snapshot->map_h &&
           source_terrain_revision == snapshot->terrain_revision &&
           source_coast_revision == snapshot->coast_revision;
}

static int tile_is_land(const RenderSnapshot *snapshot, int index) {
    return is_land((Geography)snapshot->tiles[index].geography);
}

int map_shore_color_cache_prepare(const RenderSnapshot *snapshot) {
    static const signed char neighbors[8][2] = {
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    };
    int *queue;
    int count;
    int head = 0;
    int tail = 0;
    int i;
    if (!snapshot || !snapshot->world_generated || snapshot->map_w <= 0 ||
        snapshot->map_h <= 0) return 0;
    if (source_matches(snapshot)) return 1;
    count = snapshot->map_w * snapshot->map_h;
    if (count > donor_capacity) {
        int *replacement = (int *)realloc(
            donor_indices, (size_t)count * sizeof(*donor_indices));
        if (!replacement) return 0;
        donor_indices = replacement;
        donor_capacity = count;
    }
    queue = (int *)malloc((size_t)count * sizeof(*queue));
    if (!queue) return 0;
    for (i = 0; i < count; i++) {
        const SnapshotTile *tile = &snapshot->tiles[i];
        donor_indices[i] = -1;
        if (tile_is_land(snapshot, i) && donor_geography(tile->geography)) {
            donor_indices[i] = i;
            queue[tail++] = i;
        }
    }
    while (head < tail) {
        int index = queue[head++];
        int x = index % snapshot->map_w;
        int y = index / snapshot->map_w;
        int n;
        for (n = 0; n < 8; n++) {
            int nx = x + neighbors[n][0];
            int ny = y + neighbors[n][1];
            int neighbor;
            if (nx < 0 || ny < 0 || nx >= snapshot->map_w ||
                ny >= snapshot->map_h) continue;
            neighbor = ny * snapshot->map_w + nx;
            if (donor_indices[neighbor] >= 0 ||
                !tile_is_land(snapshot, neighbor)) continue;
            donor_indices[neighbor] = donor_indices[index];
            queue[tail++] = neighbor;
        }
    }
    free(queue);
    source_map_w = snapshot->map_w;
    source_map_h = snapshot->map_h;
    source_terrain_revision = snapshot->terrain_revision;
    source_coast_revision = snapshot->coast_revision;
    source_valid = 1;
    stats.rebuilds++;
    stats.tile_scans += (uint64_t)count;
    stats.retained_bytes = (uint64_t)donor_capacity * sizeof(*donor_indices);
    return 1;
}

const SnapshotTile *map_shore_color_cache_island_donor(
    const RenderSnapshot *snapshot, const SnapshotTile *tile) {
    ptrdiff_t index;
    int donor;
    if (!source_matches(snapshot) || !tile || tile->geography != GEO_ISLAND)
        return NULL;
    index = tile - snapshot->tiles;
    if (index < 0 || index >= (ptrdiff_t)(snapshot->map_w * snapshot->map_h))
        return NULL;
    donor = donor_indices[index];
    if (donor < 0 || donor >= snapshot->map_w * snapshot->map_h)
        return NULL;
    return &snapshot->tiles[donor];
}

const MapShoreColorCacheStats *map_shore_color_cache_stats(void) {
    return &stats;
}

void map_shore_color_cache_invalidate(void) {
    free(donor_indices);
    donor_indices = NULL;
    donor_capacity = 0;
    source_map_w = source_map_h = 0;
    source_terrain_revision = source_coast_revision = 0;
    source_valid = 0;
    memset(&stats, 0, sizeof(stats));
}
