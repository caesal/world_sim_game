#include "core/render_snapshot_river.h"

#include "core/render_snapshot_river_lakes.h"
#include "core/worldgen_fault_injection.h"
#include "world/river_presentation_state.h"
#include "world/river_path_validation.h"
#include "world/world_physical_state.h"

#include <stdlib.h>
#include <string.h>

int render_snapshot_river_reserve(SnapshotRiverField *field, int capacity) {
    SnapshotRiverPath *paths;
    if (!field || !river_path_count_valid(capacity, MAX_MAP_W, MAX_MAP_H)) return 0;
    if (capacity == 0) {
        free(field->paths);
        field->paths = NULL;
        field->valid = 0;
        field->path_count = 0;
        field->capacity = 0;
        return 1;
    }
    if (field->paths && field->capacity == capacity) {
        field->valid = 0;
        field->path_count = capacity;
        return 1;
    }
    paths = (SnapshotRiverPath *)realloc(
        field->paths, (size_t)capacity * sizeof(*field->paths));
    if (!paths) return 0;
    field->paths = paths;
    field->valid = 0;
    field->path_count = capacity;
    field->capacity = capacity;
    return 1;
}

void render_snapshot_river_release(SnapshotRiverField *field) {
    if (!field) return;
    free(field->paths);
    memset(field, 0, sizeof(*field));
}

size_t render_snapshot_river_field_retained_bytes(
    const SnapshotRiverField *field) {
    return field && field->capacity > 0 ?
        (size_t)field->capacity * sizeof(*field->paths) : 0;
}

int render_snapshot_river_clone(SnapshotRiverField *out,
                                const SnapshotRiverField *source) {
    if (!out || !source) return 0;
    if (source->path_count < 0 || source->capacity != source->path_count ||
        (source->path_count == 0 && source->paths) ||
        (source->path_count > 0 &&
         (!source->paths ||
          !river_path_count_valid(source->path_count,
                                  source->map_w, source->map_h)))) return 0;
    if (out == source) return 1;
    if (!render_snapshot_river_reserve(out, source->path_count)) return 0;
    if (source->path_count > 0) {
        memcpy(out->paths, source->paths,
               (size_t)source->path_count * sizeof(*out->paths));
    }
    out->valid = source->valid;
    out->revision = source->revision;
    out->map_w = source->map_w;
    out->map_h = source->map_h;
    out->path_count = source->path_count;
    return 1;
}

static uint8_t semantic_flags_at(int x, int y) {
    const WorldPhysicalTileState *tile = world_physical_state_tile_at(x, y);
    uint8_t result = 0;
    if (!tile) return 0;
    if (tile->river_flags & WORLD_RIVER_TILE_SOURCE) result |= SNAPSHOT_RIVER_SOURCE;
    if (tile->river_flags & WORLD_RIVER_TILE_CONFLUENCE) result |= SNAPSHOT_RIVER_CONFLUENCE;
    if (tile->river_flags & WORLD_RIVER_TILE_LAKE) result |= SNAPSHOT_RIVER_LAKE;
    if (tile->river_flags & WORLD_RIVER_TILE_MOUTH) result |= SNAPSHOT_RIVER_MOUTH;
    if (tile->river_flags & WORLD_RIVER_TILE_DELTA) result |= SNAPSHOT_RIVER_DELTA;
    if (tile->river_flags & WORLD_RIVER_TILE_CLOSED_BASIN) result |= SNAPSHOT_RIVER_CLOSED_BASIN;
    if (tile->river_flags & WORLD_RIVER_TILE_DISTRIBUTARY) result |= SNAPSHOT_RIVER_DISTRIBUTARY;
    if (tile->river_flags & WORLD_RIVER_TILE_SALT_LAKE) result |= SNAPSHOT_RIVER_SALT_LAKE;
    return result;
}

static int copy_path(SnapshotRiverPath *out, const RiverPath *path,
                     int map_w, int map_h,
                     const SnapshotRiverLakeTerminalMap *lake_terminals) {
    const WorldPhysicalTileState *terminal_tile;
    int i;
    if (!out || !path || !path->active || path->point_count < 2 ||
        path->point_count > MAX_RIVER_POINTS || path->order < 0 ||
        path->order > WORLD_PHYSICAL_RIVER_ORDER_MAX || path->flow < 0 ||
        path->width < 0 || path->width > UINT16_MAX) return 0;
    memset(out, 0, sizeof(*out));
    out->flow = (uint32_t)path->flow;
    out->point_count = (uint16_t)path->point_count;
    out->width = (uint16_t)path->width;
    out->order = (uint8_t)path->order;
    terminal_tile = world_physical_state_tile_at(
        path->points[path->point_count - 2].x,
        path->points[path->point_count - 2].y);
    out->terminal_inflow = terminal_tile ? terminal_tile->river_flow : out->flow;
    for (i = 0; i < path->point_count; i++) {
        int x = path->points[i].x;
        int y = path->points[i].y;
        uint8_t flags;
        if (x < 0 || y < 0 || x >= map_w || y >= map_h) return 0;
        out->points[i].x = (uint16_t)x;
        out->points[i].y = (uint16_t)y;
        flags = semantic_flags_at(x, y);
        out->points[i].semantic_flags = flags;
        out->semantic_flags |= flags;
        if (i == path->point_count - 1) {
            out->end_flags = flags |
                render_snapshot_river_lake_terminal_flags_at(
                    lake_terminals, x, y);
        }
    }
    /* A distributary tile flag is point-local: an ordinary path may touch the
       same water tile.  Promote it to path semantics only for the exact
       two-point delta branch representation emitted by rivers.c. */
    if (out->point_count != 2 ||
        !(out->points[0].semantic_flags & SNAPSHOT_RIVER_DELTA) ||
        !(out->end_flags & SNAPSHOT_RIVER_DISTRIBUTARY)) {
        out->semantic_flags &= (uint8_t)~SNAPSHOT_RIVER_DISTRIBUTARY;
    }
    return 1;
}

int render_snapshot_river_copy(SnapshotRiverField *out, int revision_key,
                               int map_w, int map_h) {
    SnapshotRiverField staged = {0};
    SnapshotRiverLakeTerminalMap lake_terminals = {0};
    SnapshotRiverField previous;
    int i;
    int count = 0;
    if (!out || map_w <= 0 || map_w > MAX_MAP_W ||
        map_h <= 0 || map_h > MAX_MAP_H ||
        !river_paths_validate(river_paths, river_path_count, map_w, map_h)) return 0;
    if (worldgen_fault_injection_should_fail(
            WORLDGEN_FAULT_SNAPSHOT_RIVER_COPY)) return 0;
    if ((river_path_count > 0 &&
         !render_snapshot_river_lake_terminal_map_build(
             &lake_terminals, map_w, map_h)) ||
        !render_snapshot_river_reserve(&staged, river_path_count)) {
        render_snapshot_river_lake_terminal_map_release(&lake_terminals);
        return 0;
    }
    staged.revision = revision_key;
    staged.map_w = map_w;
    staged.map_h = map_h;
    for (i = 0; i < river_path_count; i++) {
        if (!river_paths[i].active) continue;
        if (count >= staged.capacity ||
            !copy_path(&staged.paths[count], &river_paths[i], map_w, map_h,
                       &lake_terminals)) {
            render_snapshot_river_release(&staged);
            render_snapshot_river_lake_terminal_map_release(&lake_terminals);
            return 0;
        }
        count++;
    }
    if (count != river_path_count) {
        render_snapshot_river_release(&staged);
        render_snapshot_river_lake_terminal_map_release(&lake_terminals);
        return 0;
    }
    staged.path_count = count;
    staged.valid = 1;
    previous = *out;
    *out = staged;
    render_snapshot_river_release(&previous);
    render_snapshot_river_lake_terminal_map_release(&lake_terminals);
    return 1;
}
