#ifndef WORLD_SIM_WORLD_PHYSICAL_STATE_H
#define WORLD_SIM_WORLD_PHYSICAL_STATE_H

#include <stdint.h>

#include "core/constants.h"

#define WORLD_WIND_DIRECTION_COUNT 16
#define WORLD_WIND_CALM_SPEED 7
#define WORLD_PHYSICAL_RIVER_ORDER_MAX 255

typedef enum {
    WORLD_RIVER_TILE_CHANNEL = 1 << 0,
    WORLD_RIVER_TILE_LAKE = 1 << 1,
    WORLD_RIVER_TILE_MOUTH = 1 << 2,
    WORLD_RIVER_TILE_DELTA = 1 << 3,
    WORLD_RIVER_TILE_CONFLUENCE = 1 << 4,
    WORLD_RIVER_TILE_CLOSED_BASIN = 1 << 5,
    WORLD_RIVER_TILE_SOURCE = 1 << 6,
    WORLD_RIVER_TILE_DISTRIBUTARY = 1 << 7,
    WORLD_RIVER_TILE_SALT_LAKE = 1 << 8
} WorldRiverTileFlags;

#define WORLD_RIVER_TILE_FLAG_MASK 0x1ffu

typedef struct {
    uint32_t river_flow;
    uint16_t river_width;
    uint8_t wind_direction16;
    uint8_t wind_speed;
    uint8_t soil_fertility;
    uint8_t river_order;
    uint16_t river_flags;
} WorldPhysicalTileState;

typedef struct {
    int map_w;
    int map_h;
    int tile_count;
    const uint32_t *river_flow;
    const uint16_t *river_width;
    const uint8_t *wind_direction16;
    const uint8_t *wind_speed;
    const uint8_t *soil_fertility;
    const uint8_t *river_order;
    const uint16_t *river_flags;
} WorldPhysicalStateInput;

void world_physical_state_reset(void);
int world_physical_state_commit(int map_w, int map_h,
                                const WorldPhysicalTileState *tiles,
                                int tile_count);
int world_physical_state_commit_fields(const WorldPhysicalStateInput *input);
int world_physical_state_validate_fields(const WorldPhysicalStateInput *input);
int world_physical_state_validate(int map_w, int map_h,
                                  const WorldPhysicalTileState *tiles,
                                  int tile_count);

int world_physical_state_valid(void);
int world_physical_state_revision(void);
int world_physical_state_width(void);
int world_physical_state_height(void);
int world_physical_state_tile_count(void);
const WorldPhysicalTileState *world_physical_state_tiles(void);
const WorldPhysicalTileState *world_physical_state_tile_at(int x, int y);

int world_physical_state_wind_sample_count(int lod);
int world_physical_state_wind_sample(int lod, int index,
                                     int *x, int *y,
                                     int *direction, int *speed);

#endif
