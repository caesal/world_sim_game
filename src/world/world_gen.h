#ifndef WORLD_SIM_WORLD_GEN_H
#define WORLD_SIM_WORLD_GEN_H

#include <stddef.h>
#include <stdint.h>

#include "core/constants.h"

typedef struct WorldGenContext WorldGenContext;

enum {
    WORLD_GEN_DEFAULT_OCEAN = 50,
    WORLD_GEN_DEFAULT_CONTINENT = 50,
    WORLD_GEN_DEFAULT_RELIEF = 50,
    WORLD_GEN_DEFAULT_MOISTURE = 50,
    WORLD_GEN_DEFAULT_DROUGHT = 50,
    WORLD_GEN_DEFAULT_VEGETATION = 50,
    WORLD_GEN_DEFAULT_BIAS_FOREST = 50,
    WORLD_GEN_DEFAULT_BIAS_DESERT = 50,
    WORLD_GEN_DEFAULT_BIAS_MOUNTAIN = 50,
    WORLD_GEN_DEFAULT_BIAS_WETLAND = 50
};

typedef struct {
    int ocean;
    int continent;
    int relief;
    int moisture;
    int drought;
    int vegetation;
    int bias_forest;
    int bias_desert;
    int bias_mountain;
    int bias_wetland;
    unsigned int seed;
    int random_seed;
} WorldGenConfig;

extern const WorldGenConfig DEFAULT_WORLD_GEN_CONFIG;

typedef struct {
    uint64_t elevation_ms;
    uint64_t mountain_ms;
    uint64_t climate_ms;
    uint64_t hydrology_ms;
    uint64_t classification_ms;
    uint64_t commit_ms;
    uint64_t total_ms;
    size_t context_bytes;
    size_t hydrology_bytes;
    size_t staged_path_bytes;
    size_t peak_bytes;
    uint64_t physical_hash;
    int land_tiles;
    int ocean_tiles;
    int mountain_tiles;
    int river_tiles;
    int river_segments_required;
    int river_segments_copied;
} WorldGenDiagnostics;

WorldGenContext *world_gen_prepare_with_config(const WorldGenConfig *config);
WorldGenContext *world_gen_prepare_for_dimensions(const WorldGenConfig *config,
                                                  int width, int height);
int world_gen_run_prepared(WorldGenContext *context);
int world_gen_prepared_can_commit(const WorldGenContext *context,
                                  int width, int height);
int world_gen_commit_prepared(WorldGenContext *context);
void world_gen_release_prepared(WorldGenContext *context);
const WorldGenDiagnostics *world_gen_last_diagnostics(void);
int world_gen_last_committed_diagnostics(WorldGenDiagnostics *out);
int world_gen_last_committed_config(WorldGenConfig *out);
void generate_world_with_config(const WorldGenConfig *config);

#endif
