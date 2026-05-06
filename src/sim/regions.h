#ifndef WORLD_SIM_REGIONS_H
#define WORLD_SIM_REGIONS_H

#include "core/world_types.h"

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
} NaturalRegion;

extern NaturalRegion natural_regions[MAX_NATURAL_REGIONS];
extern int region_count;

void regions_reset(void);
void regions_generate(int region_size_value);
const NaturalRegion *regions_get(int region_id);
int regions_select_spawn_region(int preferred_x, int preferred_y, int *out_region_id, int *out_x, int *out_y);
int regions_claim_as_province(int region_id, int owner, int city_id);
int regions_region_for_city(int city_id);
int regions_region_has_owner_neighbor(int region_id, int owner);
int regions_claim_for_civ(int region_id, int owner, int preferred_city_id, int create_city);

#endif
