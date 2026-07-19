#ifndef WORLD_SIM_RENDER_SNAPSHOT_RIVER_LAKES_H
#define WORLD_SIM_RENDER_SNAPSHOT_RIVER_LAKES_H

#include <stdint.h>

#include "world/world_physical_state.h"

typedef struct {
    int map_w;
    int map_h;
    int tile_count;
    uint8_t *terminal_flags;
} SnapshotRiverLakeTerminalMap;

int render_snapshot_river_lake_terminal_map_build(
    SnapshotRiverLakeTerminalMap *map, int map_w, int map_h);
int render_snapshot_river_lake_terminal_map_build_from_tiles(
    SnapshotRiverLakeTerminalMap *map,
    const WorldPhysicalTileState *tiles, int map_w, int map_h, int tile_count);
uint8_t render_snapshot_river_lake_terminal_flags_at(
    const SnapshotRiverLakeTerminalMap *map, int x, int y);
void render_snapshot_river_lake_terminal_map_release(
    SnapshotRiverLakeTerminalMap *map);

#endif
