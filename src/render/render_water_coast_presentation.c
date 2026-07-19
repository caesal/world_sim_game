#include "render/render_water_coast_presentation.h"
#include "world/terrain_query.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int ocean_geography(int geography) {
    return geography == GEO_OCEAN || geography == GEO_BAY;
}

static void protect_marine_tile(
    const RenderSnapshot *snapshot, unsigned char *protected_ocean,
    int x, int y, uint64_t *protected_tiles) {
    int index;
    if (x < 0 || y < 0 || x >= snapshot->map_w || y >= snapshot->map_h)
        return;
    index = y * snapshot->map_w + x;
    if (!ocean_geography(snapshot->tiles[index].geography) ||
        protected_ocean[index]) return;
    protected_ocean[index] = 1;
    (*protected_tiles)++;
}

static void protect_marine_radius(
    const RenderSnapshot *snapshot, unsigned char *protected_ocean,
    int x, int y, uint64_t *protected_tiles) {
    int dx, dy;
    for (dy = -WATER_COAST_PRESENTATION_MARINE_ANCHOR_RADIUS;
         dy <= WATER_COAST_PRESENTATION_MARINE_ANCHOR_RADIUS; dy++) {
        for (dx = -WATER_COAST_PRESENTATION_MARINE_ANCHOR_RADIUS;
             dx <= WATER_COAST_PRESENTATION_MARINE_ANCHOR_RADIUS; dx++) {
            protect_marine_tile(snapshot, protected_ocean, x + dx, y + dy,
                                protected_tiles);
        }
    }
}

int render_water_coast_presentation_find_shallow_entry(
    const RenderSnapshot *snapshot, int land_x, int land_y,
    int *out_x, int *out_y) {
    int radius;
    int best_score = INT_MAX;
    int found = 0;
    if (!snapshot || !out_x || !out_y) return 0;
    for (radius = 1; radius <= 4; radius++) {
        int dy, dx;
        for (dy = -radius; dy <= radius; dy++) {
            for (dx = -radius; dx <= radius; dx++) {
                int nx = land_x + dx;
                int ny = land_y + dy;
                const SnapshotTile *tile;
                int score;
                if (nx < 0 || ny < 0 || nx >= snapshot->map_w ||
                    ny >= snapshot->map_h) continue;
                tile = &snapshot->tiles[ny * snapshot->map_w + nx];
                if (!ocean_geography(tile->geography) ||
                    tile->water_depth != WATER_DEPTH_SHALLOW) continue;
                score = abs(dx) + abs(dy);
                if (score >= best_score) continue;
                best_score = score;
                *out_x = nx;
                *out_y = ny;
                found = 1;
            }
        }
        if (found) return 1;
    }
    return 0;
}

int render_water_coast_presentation_build_marine_protection(
    const RenderSnapshot *snapshot, unsigned char *protected_ocean,
    uint64_t *protected_tiles) {
    const int river_flags = SNAPSHOT_RIVER_MOUTH | SNAPSHOT_RIVER_DELTA |
                            SNAPSHOT_RIVER_DISTRIBUTARY;
    size_t count;
    int i, x, y;
    if (!snapshot || !protected_ocean || !protected_tiles ||
        !snapshot->world_generated || snapshot->map_w <= 0 ||
        snapshot->map_h <= 0) return 0;
    count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    memset(protected_ocean, 0, count);
    *protected_tiles = 0;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *tile =
                &snapshot->tiles[y * snapshot->map_w + x];
            if (ocean_geography(tile->geography) &&
                (x == 0 || y == 0 || x == snapshot->map_w - 1 ||
                 y == snapshot->map_h - 1)) {
                protect_marine_tile(snapshot, protected_ocean, x, y,
                                    protected_tiles);
            }
            if (tile->geography == GEO_DELTA)
                protect_marine_radius(snapshot, protected_ocean, x, y,
                                      protected_tiles);
        }
    }
    if (snapshot->rivers.valid && snapshot->rivers.path_count > 0 &&
        snapshot->rivers.capacity == snapshot->rivers.path_count &&
        snapshot->rivers.paths) {
        int path_count = snapshot->rivers.path_count;
        for (i = 0; i < path_count; i++) {
            const SnapshotRiverPath *path = &snapshot->rivers.paths[i];
            int point_count = path->point_count;
            int p;
            if (point_count <= 0 || point_count > MAX_RIVER_POINTS) continue;
            for (p = 0; p < point_count; p++) {
                const SnapshotRiverPoint *point = &path->points[p];
                if (!(point->semantic_flags & river_flags)) continue;
                protect_marine_radius(snapshot, protected_ocean,
                                      point->x, point->y, protected_tiles);
            }
            if (point_count > 0 && (path->end_flags & river_flags)) {
                int p0 = point_count > 4 ? point_count - 4 : 0;
                int p;
                for (p = p0; p < point_count; p++) {
                    const SnapshotRiverPoint *point = &path->points[p];
                    protect_marine_radius(snapshot, protected_ocean,
                                          point->x, point->y,
                                          protected_tiles);
                }
            }
        }
    }
    for (i = 0; i < snapshot->region_count; i++) {
        const SnapshotRegion *region = &snapshot->regions[i];
        int sea_x, sea_y;
        if (!region->has_port_site) continue;
        if (!render_water_coast_presentation_find_shallow_entry(
                snapshot, region->port_x, region->port_y,
                &sea_x, &sea_y)) continue;
        protect_marine_radius(snapshot, protected_ocean,
                              sea_x, sea_y, protected_tiles);
    }
    return 1;
}

static int touches_retained_ocean(const unsigned char *ocean_mask,
                                  const unsigned char *suppressed_tiles,
                                  int width, int height, int index) {
    static const int dx[4] = {0, 1, 0, -1};
    static const int dy[4] = {-1, 0, 1, 0};
    int x = index % width;
    int y = index / width;
    int n;
    if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
        return 1;
    for (n = 0; n < 4; n++) {
        int neighbor = (y + dy[n]) * width + x + dx[n];
        if (ocean_mask[neighbor] && suppressed_tiles[neighbor] == 0)
            return 1;
    }
    return 0;
}

int render_water_coast_presentation_preserve_anchored_components(
    const unsigned char *ocean_mask, const unsigned char *protected_ocean,
    int width, int height,
    unsigned char *suppressed_tiles, uint64_t *protected_components,
    uint64_t *protected_tiles, uint64_t *transient_bytes) {
    static const int dx[4] = {0, 1, 0, -1};
    static const int dy[4] = {-1, 0, 1, 0};
    int *component_queue = NULL;
    int *search_queue = NULL;
    int *parent = NULL;
    size_t count;
    int start;
    if (!ocean_mask || !protected_ocean || !suppressed_tiles ||
        !protected_components || !protected_tiles || !transient_bytes ||
        width <= 0 || height <= 0) return 0;
    count = (size_t)width * (size_t)height;
    if (count > INT_MAX) return 0;
    component_queue = (int *)malloc(count * sizeof(*component_queue));
    search_queue = (int *)malloc(count * sizeof(*search_queue));
    parent = (int *)malloc(count * sizeof(*parent));
    if (!component_queue || !search_queue || !parent) goto failure;
    *protected_components = 0;
    *protected_tiles = 0;
    *transient_bytes = count * (sizeof(*component_queue) +
                                sizeof(*search_queue) + sizeof(*parent));
    for (start = 0; start < (int)count; start++) {
        int head = 0;
        int tail = 0;
        int search_head = 0;
        int search_tail = 0;
        uint64_t restored = 0;
        int q;
        if (suppressed_tiles[start] != 1) continue;
        suppressed_tiles[start] = 2;
        component_queue[tail++] = start;
        while (head < tail) {
            int index = component_queue[head++];
            int x = index % width;
            int y = index / width;
            int n;
            for (n = 0; n < 4; n++) {
                int nx = x + dx[n];
                int ny = y + dy[n];
                int neighbor;
                if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                    continue;
                neighbor = ny * width + nx;
                if (suppressed_tiles[neighbor] != 1) continue;
                suppressed_tiles[neighbor] = 2;
                component_queue[tail++] = neighbor;
            }
        }
        for (q = 0; q < tail; q++) {
            int index = component_queue[q];
            parent[index] = -2;
            if (!touches_retained_ocean(ocean_mask, suppressed_tiles,
                                        width, height, index)) continue;
            parent[index] = -1;
            search_queue[search_tail++] = index;
        }
        while (search_head < search_tail) {
            int index = search_queue[search_head++];
            int x = index % width;
            int y = index / width;
            int n;
            for (n = 0; n < 4; n++) {
                int nx = x + dx[n];
                int ny = y + dy[n];
                int neighbor;
                if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                    continue;
                neighbor = ny * width + nx;
                if (suppressed_tiles[neighbor] != 2 ||
                    parent[neighbor] != -2) continue;
                parent[neighbor] = index;
                search_queue[search_tail++] = neighbor;
            }
        }
        /* Restore the deterministic shortest path from each protected anchor
           to retained ocean.  Shared path cells are counted and drawn once. */
        for (q = 0; q < tail; q++) {
            int index = component_queue[q];
            if (!protected_ocean[index] || parent[index] == -2) continue;
            while (index >= 0) {
                int next = parent[index];
                if (suppressed_tiles[index] == 2) {
                    suppressed_tiles[index] = 0;
                    restored++;
                }
                index = next;
            }
        }
        for (q = 0; q < tail; q++)
            if (suppressed_tiles[component_queue[q]] == 2)
                suppressed_tiles[component_queue[q]] = 1;
        if (restored) {
            (*protected_components)++;
            *protected_tiles += restored;
        }
    }
    free(parent);
    free(search_queue);
    free(component_queue);
    return 1;
failure:
    free(parent);
    free(search_queue);
    free(component_queue);
    return 0;
}

static uint64_t hash_categories(const unsigned char *categories,
                                size_t count) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t i;
    for (i = 0; i < count; i++) {
        hash ^= categories[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int render_water_coast_presentation_build(
    const RenderSnapshot *snapshot, unsigned char *categories,
    RenderWaterCoastPresentationMetrics *metrics) {
    size_t count;
    size_t i;
    if (!snapshot || !categories || !metrics || !snapshot->world_generated ||
        snapshot->map_w <= 0 || snapshot->map_h <= 0) return 0;
    count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    memset(metrics, 0, sizeof(*metrics));
    for (i = 0; i < count; i++) {
        int geography = snapshot->tiles[i].geography;
        if (geography == GEO_LAKE) {
            categories[i] = WATER_COAST_PRESENTATION_LAKE;
            metrics->semantic_lake_tiles++;
        } else if (ocean_geography(geography)) {
            categories[i] = WATER_COAST_PRESENTATION_OCEAN;
            metrics->semantic_ocean_tiles++;
        } else {
            categories[i] = WATER_COAST_PRESENTATION_LAND;
            metrics->semantic_land_tiles++;
        }
    }
    /* Coast categories are now authoritative generation output.  Rendering
       may rasterize coverage at any LOD, but it must not delete semantic
       ocean or fill semantic land to repair a generated mask. */
    metrics->presentation_hash = hash_categories(categories, count);
    return 1;
}
