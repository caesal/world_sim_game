#include "core/render_snapshot_river_lakes.h"

#include "core/render_snapshot_river_types.h"

#include <stdlib.h>
#include <string.h>

#define LAKE_UNVISITED UINT8_C(0x01)

static uint8_t component_terminal_flags(uint16_t river_flags) {
    uint8_t flags = 0;
    if (river_flags & WORLD_RIVER_TILE_CLOSED_BASIN)
        flags |= SNAPSHOT_RIVER_CLOSED_BASIN;
    if (river_flags & WORLD_RIVER_TILE_SALT_LAKE)
        flags |= SNAPSHOT_RIVER_SALT_LAKE;
    return flags;
}

void render_snapshot_river_lake_terminal_map_release(
    SnapshotRiverLakeTerminalMap *map) {
    if (!map) return;
    free(map->terminal_flags);
    memset(map, 0, sizeof(*map));
}

int render_snapshot_river_lake_terminal_map_build_from_tiles(
    SnapshotRiverLakeTerminalMap *map,
    const WorldPhysicalTileState *tiles, int map_w, int map_h, int tile_count) {
    static const int dx[4] = {1, 0, -1, 0};
    static const int dy[4] = {0, 1, 0, -1};
    uint8_t *terminal_flags;
    int *queue;
    int index;
    if (!map || !tiles || map_w <= 0 || map_w > MAX_MAP_W ||
        map_h <= 0 || map_h > MAX_MAP_H || tile_count != map_w * map_h) return 0;
    terminal_flags = (uint8_t *)calloc((size_t)tile_count, 1);
    queue = (int *)malloc((size_t)tile_count * sizeof(*queue));
    if (!terminal_flags || !queue) {
        free(terminal_flags);
        free(queue);
        return 0;
    }
    for (index = 0; index < tile_count; index++) {
        if (tiles[index].river_flags & WORLD_RIVER_TILE_LAKE)
            terminal_flags[index] = LAKE_UNVISITED;
    }
    for (index = 0; index < tile_count; index++) {
        int head = 0, tail = 0;
        uint8_t flags = 0;
        if (terminal_flags[index] != LAKE_UNVISITED) continue;
        terminal_flags[index] = 0;
        queue[tail++] = index;
        while (head < tail) {
            int current = queue[head++];
            int x = current % map_w;
            int y = current / map_w;
            int direction;
            flags |= component_terminal_flags(tiles[current].river_flags);
            for (direction = 0; direction < 4; direction++) {
                int nx = x + dx[direction];
                int ny = y + dy[direction];
                int neighbor;
                if (nx < 0 || ny < 0 || nx >= map_w || ny >= map_h) continue;
                neighbor = ny * map_w + nx;
                if (terminal_flags[neighbor] != LAKE_UNVISITED) continue;
                terminal_flags[neighbor] = 0;
                queue[tail++] = neighbor;
            }
        }
        while (tail > 0) terminal_flags[queue[--tail]] = flags;
    }
    free(queue);
    render_snapshot_river_lake_terminal_map_release(map);
    map->map_w = map_w;
    map->map_h = map_h;
    map->tile_count = tile_count;
    map->terminal_flags = terminal_flags;
    return 1;
}

int render_snapshot_river_lake_terminal_map_build(
    SnapshotRiverLakeTerminalMap *map, int map_w, int map_h) {
    if (!world_physical_state_valid() ||
        world_physical_state_width() != map_w ||
        world_physical_state_height() != map_h) return 0;
    return render_snapshot_river_lake_terminal_map_build_from_tiles(
        map, world_physical_state_tiles(), map_w, map_h,
        world_physical_state_tile_count());
}

uint8_t render_snapshot_river_lake_terminal_flags_at(
    const SnapshotRiverLakeTerminalMap *map, int x, int y) {
    if (!map || !map->terminal_flags || x < 0 || y < 0 ||
        x >= map->map_w || y >= map->map_h) return 0;
    return map->terminal_flags[y * map->map_w + x];
}
