#include "io/map_save_regions.h"

#include "sim/regions.h"

#include <string.h>

typedef struct {
    int id;
    int alive;
    int tile_count;
    int owner_civ;
    int city_id;
    int center_x;
    int center_y;
    int capital_x;
    int capital_y;
    int port_x;
    int port_y;
    int has_port_site;
    int neighbor_count;
    int neighbors[MAX_REGION_NEIGHBORS];
    TerrainStats average_stats;
    TerrainStats total_stats;
    Geography dominant_geography;
    Climate dominant_climate;
    Ecology dominant_ecology;
    int natural_defense;
    int movement_difficulty;
    int habitability;
    int development_score;
    int cradle_score;
    int resource_diversity;
    int viable_direction_count;
    int direction_score[REGION_DIR_COUNT];
} LegacyNaturalRegionV4;

static int write_block(FILE *file, const void *data, size_t size, size_t count) {
    return fwrite(data, size, count, file) == count;
}

static int read_block(FILE *file, void *data, size_t size, size_t count) {
    return fread(data, size, count, file) == count;
}

int map_save_write_natural_regions(FILE *file) {
    return write_block(file, natural_regions, sizeof(NaturalRegion), (size_t)region_count);
}

int map_save_read_natural_regions(FILE *file, int save_version) {
    if (save_version >= 5) {
        return read_block(file, natural_regions, sizeof(NaturalRegion), (size_t)region_count);
    }
    {
        LegacyNaturalRegionV4 legacy[MAX_NATURAL_REGIONS];
        int i;

        if (!read_block(file, legacy, sizeof(LegacyNaturalRegionV4), (size_t)region_count)) return 0;
        for (i = 0; i < region_count; i++) {
            memset(&natural_regions[i], 0, sizeof(natural_regions[i]));
            memcpy(&natural_regions[i], &legacy[i], sizeof(legacy[i]));
            natural_regions[i].disconnected_months = 0;
            natural_regions[i].disconnected_component_id = -1;
        }
        return 1;
    }
}
