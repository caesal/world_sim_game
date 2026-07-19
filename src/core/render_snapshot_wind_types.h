#ifndef WORLD_SIM_RENDER_SNAPSHOT_WIND_TYPES_H
#define WORLD_SIM_RENDER_SNAPSHOT_WIND_TYPES_H

#include <stdint.h>

#include "core/wind_sample_contract.h"

enum {
    SNAPSHOT_WIND_LOD_COARSE = 0,
    SNAPSHOT_WIND_LOD_MEDIUM,
    SNAPSHOT_WIND_LOD_FINE,
    SNAPSHOT_WIND_LOD_COUNT
};

#define SNAPSHOT_WIND_COARSE_MAX WORLD_WIND_SAMPLE_COARSE_COUNT
#define SNAPSHOT_WIND_MEDIUM_MAX WORLD_WIND_SAMPLE_MEDIUM_COUNT
#define SNAPSHOT_WIND_FINE_MAX WORLD_WIND_SAMPLE_FINE_COUNT

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t direction;
    uint8_t speed;
} SnapshotWindSample;

typedef struct {
    int valid;
    int revision;
    int map_w;
    int map_h;
    int coarse_count;
    int medium_count;
    int fine_count;
    SnapshotWindSample coarse[SNAPSHOT_WIND_COARSE_MAX];
    SnapshotWindSample medium[SNAPSHOT_WIND_MEDIUM_MAX];
    SnapshotWindSample fine[SNAPSHOT_WIND_FINE_MAX];
} SnapshotWindField;

#endif
