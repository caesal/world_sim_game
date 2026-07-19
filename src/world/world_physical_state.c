#include "world/world_physical_state.h"

#include "core/wind_sample_contract.h"

#include <string.h>

enum {
    WIND_LOD_COUNT = 3,
    WIND_COARSE_MAX = WORLD_WIND_SAMPLE_COARSE_COUNT,
    WIND_MEDIUM_MAX = WORLD_WIND_SAMPLE_MEDIUM_COUNT,
    WIND_FINE_MAX = WORLD_WIND_SAMPLE_FINE_COUNT
};

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t direction;
    uint8_t speed;
} WorldWindSample;

static WorldPhysicalTileState physical_tiles[MAX_MAP_W * MAX_MAP_H];
static WorldWindSample coarse_samples[WIND_COARSE_MAX];
static WorldWindSample medium_samples[WIND_MEDIUM_MAX];
static WorldWindSample fine_samples[WIND_FINE_MAX];
static int sample_counts[WIND_LOD_COUNT];
static int physical_w;
static int physical_h;
static int physical_count;
static int physical_valid;
static int physical_revision = 1;

static void bump_revision(void) {
    physical_revision++;
    if (physical_revision <= 0) physical_revision = 1;
}

static int dimensions_valid(int width, int height, int count) {
    if (width <= 0 || width > MAX_MAP_W || height <= 0 || height > MAX_MAP_H) return 0;
    return count == width * height;
}

static int tile_valid(const WorldPhysicalTileState *tile) {
    if (!tile || tile->wind_direction16 >= WORLD_WIND_DIRECTION_COUNT ||
        tile->wind_speed > 100 || tile->soil_fertility > 100 ||
        (tile->river_flags & ~WORLD_RIVER_TILE_FLAG_MASK) != 0) return 0;
    if (tile->river_flow == 0 &&
        (tile->river_order != 0 || tile->river_width != 0 ||
         (tile->river_flags & WORLD_RIVER_TILE_CHANNEL) != 0)) return 0;
    if ((tile->river_flags & WORLD_RIVER_TILE_CHANNEL) != 0 &&
        (tile->river_order == 0 || tile->river_width == 0)) return 0;
    return 1;
}

int world_physical_state_validate(int width, int height,
                                  const WorldPhysicalTileState *tiles,
                                  int count) {
    int i;
    if (!tiles || !dimensions_valid(width, height, count)) return 0;
    for (i = 0; i < count; i++) {
        if (!tile_valid(&tiles[i])) return 0;
    }
    return 1;
}

static int append_sample(WorldWindSample *samples, int count, int capacity,
                         int x, int y) {
    const WorldPhysicalTileState *tile = &physical_tiles[y * physical_w + x];
    if (count >= capacity) return count;
    samples[count].x = (uint16_t)x;
    samples[count].y = (uint16_t)y;
    samples[count].direction = tile->wind_direction16;
    samples[count].speed = tile->wind_speed;
    return count + 1;
}

static int build_sample_grid(WorldWindSample *samples, int capacity,
                             int columns, int rows) {
    int gy;
    int count = 0;
    for (gy = 0; gy < rows; gy++) {
        int gx;
        int y = ((gy * 2 + 1) * physical_h) / (rows * 2);
        if (y >= physical_h) y = physical_h - 1;
        for (gx = 0; gx < columns; gx++) {
            int x = ((gx * 2 + 1) * physical_w) / (columns * 2);
            if (x >= physical_w) x = physical_w - 1;
            count = append_sample(samples, count, capacity, x, y);
        }
    }
    return count;
}

static void rebuild_wind_samples(void) {
    sample_counts[0] = build_sample_grid(
        coarse_samples, WIND_COARSE_MAX,
        WORLD_WIND_SAMPLE_COARSE_COLUMNS, WORLD_WIND_SAMPLE_COARSE_ROWS);
    sample_counts[1] = build_sample_grid(
        medium_samples, WIND_MEDIUM_MAX,
        WORLD_WIND_SAMPLE_MEDIUM_COLUMNS, WORLD_WIND_SAMPLE_MEDIUM_ROWS);
    sample_counts[2] = build_sample_grid(
        fine_samples, WIND_FINE_MAX,
        WORLD_WIND_SAMPLE_FINE_COLUMNS, WORLD_WIND_SAMPLE_FINE_ROWS);
}

void world_physical_state_reset(void) {
    physical_w = physical_h = physical_count = physical_valid = 0;
    memset(sample_counts, 0, sizeof(sample_counts));
    bump_revision();
}

int world_physical_state_commit(int width, int height,
                                const WorldPhysicalTileState *tiles,
                                int count) {
    if (!world_physical_state_validate(width, height, tiles, count)) return 0;
    memcpy(physical_tiles, tiles, (size_t)count * sizeof(physical_tiles[0]));
    physical_w = width;
    physical_h = height;
    physical_count = count;
    physical_valid = 1;
    rebuild_wind_samples();
    bump_revision();
    return 1;
}

int world_physical_state_validate_fields(const WorldPhysicalStateInput *input) {
    int i;
    if (!input || !dimensions_valid(input->map_w, input->map_h, input->tile_count) ||
        !input->river_flow || !input->river_width ||
        !input->wind_direction16 || !input->wind_speed || !input->soil_fertility ||
        !input->river_order || !input->river_flags) return 0;
    for (i = 0; i < input->tile_count; i++) {
        WorldPhysicalTileState tile;
        tile.river_flow = input->river_flow[i];
        tile.river_width = input->river_width[i];
        tile.wind_direction16 = input->wind_direction16[i];
        tile.wind_speed = input->wind_speed[i];
        tile.soil_fertility = input->soil_fertility[i];
        tile.river_order = input->river_order[i];
        tile.river_flags = input->river_flags[i];
        if (!tile_valid(&tile)) return 0;
    }
    return 1;
}

int world_physical_state_commit_fields(const WorldPhysicalStateInput *input) {
    int i;
    if (!world_physical_state_validate_fields(input)) return 0;
    for (i = 0; i < input->tile_count; i++) {
        WorldPhysicalTileState *tile = &physical_tiles[i];
        tile->river_flow = input->river_flow[i];
        tile->river_width = input->river_width[i];
        tile->wind_direction16 = input->wind_direction16[i];
        tile->wind_speed = input->wind_speed[i];
        tile->soil_fertility = input->soil_fertility[i];
        tile->river_order = input->river_order[i];
        tile->river_flags = input->river_flags[i];
    }
    physical_w = input->map_w;
    physical_h = input->map_h;
    physical_count = input->tile_count;
    physical_valid = 1;
    rebuild_wind_samples();
    bump_revision();
    return 1;
}

int world_physical_state_valid(void) { return physical_valid; }
int world_physical_state_revision(void) { return physical_revision; }
int world_physical_state_width(void) { return physical_w; }
int world_physical_state_height(void) { return physical_h; }
int world_physical_state_tile_count(void) { return physical_count; }
const WorldPhysicalTileState *world_physical_state_tiles(void) {
    return physical_valid ? physical_tiles : NULL;
}

const WorldPhysicalTileState *world_physical_state_tile_at(int x, int y) {
    if (!physical_valid || x < 0 || y < 0 || x >= physical_w || y >= physical_h) return NULL;
    return &physical_tiles[y * physical_w + x];
}

static const WorldWindSample *sample_list(int lod) {
    if (lod == 0) return coarse_samples;
    if (lod == 1) return medium_samples;
    if (lod == 2) return fine_samples;
    return NULL;
}

int world_physical_state_wind_sample_count(int lod) {
    return physical_valid && lod >= 0 && lod < WIND_LOD_COUNT ? sample_counts[lod] : 0;
}

int world_physical_state_wind_sample(int lod, int index,
                                     int *x, int *y,
                                     int *direction, int *speed) {
    const WorldWindSample *samples = sample_list(lod);
    const WorldWindSample *sample;
    if (!physical_valid || !samples || index < 0 || index >= sample_counts[lod]) return 0;
    sample = &samples[index];
    if (x) *x = sample->x;
    if (y) *y = sample->y;
    if (direction) *direction = sample->direction;
    if (speed) *speed = sample->speed;
    return 1;
}
