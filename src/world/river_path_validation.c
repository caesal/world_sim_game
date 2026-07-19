#include "world/river_path_validation.h"
#include "world/river_types.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

/* Bound raw v20 river payloads independently of map topology so a malformed
   count cannot request multi-GB storage. */
#define RIVER_PATH_STORAGE_BYTE_LIMIT (128u * 1024u * 1024u)

static int path_points_valid(const RiverPath *path, int map_w, int map_h) {
    int point;
    for (point = 0; point < path->point_count; point++) {
        int x = path->points[point].x;
        int y = path->points[point].y;
        int prior;
        if (x < 0 || x >= map_w || y < 0 || y >= map_h) return 0;
        if (point > 0) {
            int dx = abs(x - path->points[point - 1].x);
            int dy = abs(y - path->points[point - 1].y);
            if ((dx == 0 && dy == 0) || dx > 1 || dy > 1) return 0;
        }
        for (prior = 0; prior + 1 < point; prior++) {
            if (path->points[prior].x == x && path->points[prior].y == y) return 0;
        }
    }
    return 1;
}

int river_path_count_limit(int map_w, int map_h) {
    uint64_t semantic_limit;
    size_t storage_limit;
    if (map_w <= 0 || map_w > MAX_MAP_W ||
        map_h <= 0 || map_h > MAX_MAP_H) return -1;
    /* Every presentation path owns at least one semantic edge. A tile emits
       one ordinary edge or at most three delta branches, never both. */
    semantic_limit = (uint64_t)(unsigned int)map_w *
                     (uint64_t)(unsigned int)map_h * RIVER_DELTA_BRANCH_MAX;
    storage_limit = river_path_storage_byte_limit() / sizeof(RiverPath);
    if (semantic_limit > storage_limit) semantic_limit = storage_limit;
    if (semantic_limit > INT_MAX) semantic_limit = INT_MAX;
    return (int)semantic_limit;
}

int river_path_count_valid(int count, int map_w, int map_h) {
    int limit = river_path_count_limit(map_w, map_h);
    return limit >= 0 && count >= 0 && count <= limit;
}

size_t river_path_storage_byte_limit(void) {
    return (size_t)RIVER_PATH_STORAGE_BYTE_LIMIT;
}

int river_path_storage_bytes_checked(int count, size_t *out_bytes) {
    size_t bytes;
    if (!out_bytes || count < 0 ||
        (size_t)count > SIZE_MAX / sizeof(RiverPath)) return 0;
    bytes = (size_t)count * sizeof(RiverPath);
    if (bytes > river_path_storage_byte_limit()) return 0;
    *out_bytes = bytes;
    return 1;
}

int river_paths_validate(const RiverPath *paths, int count,
                         int map_w, int map_h) {
    int index;
    if (!river_path_count_valid(count, map_w, map_h) ||
        (count > 0 && !paths)) return 0;
    for (index = 0; index < count; index++) {
        const RiverPath *path = &paths[index];
        if (path->active != 1 || path->point_count < 2 ||
            path->point_count > MAX_RIVER_POINTS || path->flow <= 0 ||
            path->width <= 0 || path->width > UINT16_MAX ||
            path->order <= 0 || path->order > UINT8_MAX ||
            !path_points_valid(path, map_w, map_h)) return 0;
    }
    return 1;
}
