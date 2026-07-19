#ifndef WORLD_SIM_RENDER_SNAPSHOT_RIVER_TYPES_H
#define WORLD_SIM_RENDER_SNAPSHOT_RIVER_TYPES_H

#include <stdint.h>

#include "core/constants.h"

typedef enum {
    SNAPSHOT_RIVER_SOURCE = 1 << 0,
    SNAPSHOT_RIVER_CONFLUENCE = 1 << 1,
    SNAPSHOT_RIVER_LAKE = 1 << 2,
    SNAPSHOT_RIVER_MOUTH = 1 << 3,
    SNAPSHOT_RIVER_DELTA = 1 << 4,
    SNAPSHOT_RIVER_CLOSED_BASIN = 1 << 5,
    SNAPSHOT_RIVER_DISTRIBUTARY = 1 << 6,
    SNAPSHOT_RIVER_SALT_LAKE = 1 << 7
} SnapshotRiverSemanticFlags;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t semantic_flags;
} SnapshotRiverPoint;

typedef struct {
    uint32_t flow;
    uint32_t terminal_inflow;
    uint16_t point_count;
    uint16_t width;
    uint8_t order;
    uint8_t semantic_flags;
    uint8_t end_flags;
    uint8_t reserved;
    SnapshotRiverPoint points[MAX_RIVER_POINTS];
} SnapshotRiverPath;

typedef struct {
    int valid;
    int revision;
    int map_w;
    int map_h;
    int path_count;
    int capacity;
    SnapshotRiverPath *paths;
} SnapshotRiverField;

#endif
