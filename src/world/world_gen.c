#include "world/world_gen.h"

#include "core/game_state.h"
#include "core/worldgen_attempt.h"
#include "core/worldgen_fault_injection.h"
#include "world/mountain_gen.h"
#include "world/river_path_validation.h"
#include "world/river_presentation_state.h"
#include "world/rivers.h"
#include "world/terrain_query.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_climate.h"
#include "world/world_gen_context.h"
#include "world/world_gen_diagnostics.h"
#include "world/world_gen_elevation.h"
#include "world/world_gen_land_mask.h"
#include "world/world_physical_state.h"
#include "world/world_seed.h"

#include <stdlib.h>
#include <string.h>

const WorldGenConfig DEFAULT_WORLD_GEN_CONFIG = {
    WORLD_GEN_DEFAULT_OCEAN, WORLD_GEN_DEFAULT_CONTINENT, WORLD_GEN_DEFAULT_RELIEF,
    WORLD_GEN_DEFAULT_MOISTURE, WORLD_GEN_DEFAULT_DROUGHT, WORLD_GEN_DEFAULT_VEGETATION,
    WORLD_GEN_DEFAULT_BIAS_FOREST, WORLD_GEN_DEFAULT_BIAS_DESERT,
    WORLD_GEN_DEFAULT_BIAS_MOUNTAIN, WORLD_GEN_DEFAULT_BIAS_WETLAND, 0u, 1
};

static WorldGenDiagnostics last_diagnostics;
static WorldGenDiagnostics committed_diagnostics;
static int committed_diagnostics_valid;
static WorldGenConfig last_config;
static int last_config_valid;
static int committed_physical_revision;

static uint64_t clock_milliseconds(void) {
    return (uint64_t)GetTickCount64();
}

static uint64_t hash_mix(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (uint8_t)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t context_physical_hash(const WorldGenContext *context) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    hash = hash_mix(hash, (uint64_t)context->width);
    hash = hash_mix(hash, (uint64_t)context->height);
    for (i = 0; i < context->tile_count; i++) {
        uint64_t terrain = (uint16_t)context->elevation[i] |
                           ((uint64_t)(uint16_t)context->moisture[i] << 16) |
                           ((uint64_t)(uint16_t)context->temperature[i] << 32) |
                           ((uint64_t)(uint16_t)context->precipitation[i] << 48);
        uint64_t labels = context->land_mask[i] |
                          ((uint64_t)context->wind_direction16[i] << 8) |
                          ((uint64_t)context->wind_speed[i] << 16) |
                          ((uint64_t)context->geography[i] << 24) |
                          ((uint64_t)context->climate[i] << 32) |
                          ((uint64_t)context->ecology[i] << 40) |
                          ((uint64_t)context->resource[i] << 48) |
                          ((uint64_t)(context->river_flags[i] & 0xffu) << 56);
        uint64_t landform = (uint16_t)context->relative_altitude[i] |
                            ((uint64_t)(uint16_t)context->slope[i] << 16) |
                            ((uint64_t)(uint16_t)context->curvature[i] << 32) |
                            ((uint64_t)context->resource_variation[i] << 48) |
                            ((uint64_t)(uint8_t)context->mountain_uplift[i] << 56);
        hash = hash_mix(hash, terrain);
        hash = hash_mix(hash, labels);
        hash = hash_mix(hash, landform);
        hash = hash_mix(hash, context->river_flags[i]);
        hash = hash_mix(hash, (uint32_t)context->drainage_receiver[i]);
        hash = hash_mix(hash, (uint32_t)context->drainage_basin[i]);
        hash = hash_mix(hash, context->river_flow[i]);
        hash = hash_mix(hash, context->river_width[i] |
                              ((uint64_t)context->river_order[i] << 16) |
                              ((uint64_t)context->soil_fertility[i] << 24));
    }
    return hash;
}

static void clear_staged_river_paths(WorldGenContext *context) {
    if (!context) return;
    free(context->staged_river_paths);
    context->staged_river_paths = NULL;
    context->staged_river_path_count = 0;
    context->staged_river_paths_required = 0;
    context->staged_river_token = 0;
    memset(&context->staged_river_diagnostics, 0,
           sizeof(context->staged_river_diagnostics));
    context->staged_river_diagnostics_valid = 0;
}

static int stage_river_paths(WorldGenContext *context) {
    RiverPath *paths = NULL;
    uint64_t staged_bytes = 0;
    int capacity = 0;
    int required;
    int copied;

    clear_staged_river_paths(context);
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_PREPARE_RIVER_PATHS);
    if (!river_network_view_matches_context(context)) {
        worldgen_attempt_note_river_paths(0, 0, 0, 0);
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_RIVER_NETWORK);
        return 0;
    }
    required = river_network_count_legacy_paths(context);
    context->staged_river_paths_required = required;
    last_diagnostics.river_segments_required = required;
    if (!river_path_count_valid(required, context->width, context->height)) {
        worldgen_attempt_note_river_paths(required, 0, 0, 0);
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_RIVER_PATH_COUNT_INVALID);
        return 0;
    }
    if (required > 0) {
        if (worldgen_fault_injection_should_fail(
                WORLDGEN_FAULT_RIVER_PATH_ALLOCATION)) {
            worldgen_attempt_note_river_paths(required, 0, 0, 0);
            worldgen_attempt_record_failure(WORLDGEN_FAILURE_RIVER_PATH_ALLOCATION);
            return 0;
        }
        if (worldgen_fault_injection_should_fail(WORLDGEN_FAULT_PREPARE_ALLOCATION)) {
            worldgen_attempt_note_river_paths(required, 0, 0, 0);
            worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARE_ALLOCATION_INJECTED);
            return 0;
        }
        paths = (RiverPath *)calloc((size_t)required, sizeof(*paths));
        if (!paths) {
            worldgen_attempt_note_river_paths(required, 0, 0, 0);
            worldgen_attempt_record_failure(WORLDGEN_FAILURE_RIVER_PATH_ALLOCATION);
            return 0;
        }
        capacity = required;
        staged_bytes = (uint64_t)(size_t)capacity * sizeof(*paths);
        last_diagnostics.staged_path_bytes = (size_t)staged_bytes;
        last_diagnostics.peak_bytes = last_diagnostics.context_bytes +
                                      last_diagnostics.hydrology_bytes +
                                      last_diagnostics.staged_path_bytes;
        worldgen_attempt_note_memory((uint64_t)last_diagnostics.peak_bytes);
        copied = worldgen_fault_injection_should_fail(
            WORLDGEN_FAULT_RIVER_PATH_COPY)
            ? 0 : river_network_copy_legacy_paths(paths, required);
        if (copied != required || !river_network_view_matches_context(context)) {
            worldgen_attempt_note_river_paths(required, copied, capacity,
                                              staged_bytes);
            free(paths);
            worldgen_attempt_record_failure(WORLDGEN_FAILURE_RIVER_PATH_COPY);
            return 0;
        }
    } else {
        copied = 0;
    }
    context->staged_river_paths = paths;
    context->staged_river_path_count = copied;
    context->staged_river_token = context->hydrology_token;
    river_generation_last_diagnostics(&context->staged_river_diagnostics);
    context->staged_river_diagnostics_valid =
        context->staged_river_diagnostics.legacy_paths_required == required &&
        context->staged_river_diagnostics.legacy_paths_truncated == 0;
    if (!context->staged_river_diagnostics_valid) {
        worldgen_attempt_note_river_paths(required, copied, capacity, staged_bytes);
        clear_staged_river_paths(context);
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_RIVER_PATH_COPY);
        return 0;
    }
    if (!river_paths_validate(paths, copied, context->width, context->height)) {
        worldgen_attempt_note_river_paths(required, copied, capacity,
                                          staged_bytes);
        clear_staged_river_paths(context);
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_RIVER_PATH_VALIDATION);
        return 0;
    }
    worldgen_attempt_note_river_paths(required, copied, capacity, staged_bytes);
    return 1;
}

int world_gen_run_prepared(WorldGenContext *context) {
    uint64_t total_start;
    uint64_t stage_start;
    uint64_t stage_ms;
    int target_land;
    int hydrology_ok;
    if (!context) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARED_VALIDATION);
        return 0;
    }
    memset(&last_diagnostics, 0, sizeof(last_diagnostics));
    last_diagnostics.context_bytes = context->allocated_bytes + sizeof(*context);
    total_start = clock_milliseconds();
    target_land = world_gen_land_mask_target_tiles(context->config.ocean,
                                                   context->tile_count);
    worldgen_attempt_note_target(context->width, context->height, target_land,
                                 context->tile_count - target_land);

    stage_start = clock_milliseconds();
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_PREPARE_ELEVATION);
    if (!world_gen_build_elevation_and_mask(context)) {
        stage_ms = clock_milliseconds() - stage_start;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_ELEVATION);
        world_gen_diagnostics_finalize_failure(
            &last_diagnostics, context, WORLDGEN_ATTEMPT_PREPARE_ELEVATION,
            stage_ms, clock_milliseconds() - total_start);
        return 0;
    }
    last_diagnostics.elevation_ms = clock_milliseconds() - stage_start;

    stage_start = clock_milliseconds();
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_PREPARE_MOUNTAINS);
    if (!world_gen_apply_mountains(context)) {
        stage_ms = clock_milliseconds() - stage_start;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_MOUNTAINS);
        world_gen_diagnostics_finalize_failure(
            &last_diagnostics, context, WORLDGEN_ATTEMPT_PREPARE_MOUNTAINS,
            stage_ms, clock_milliseconds() - total_start);
        return 0;
    }
    world_gen_finalize_elevation(context);
    last_diagnostics.mountain_ms = clock_milliseconds() - stage_start;

    stage_start = clock_milliseconds();
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_PREPARE_CLIMATE);
    if (!world_gen_build_climate_fields(context) ||
        !world_gen_classify_macro_climate(context)) {
        stage_ms = clock_milliseconds() - stage_start;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_CLIMATE);
        world_gen_diagnostics_finalize_failure(
            &last_diagnostics, context, WORLDGEN_ATTEMPT_PREPARE_CLIMATE,
            stage_ms, clock_milliseconds() - total_start);
        return 0;
    }
    last_diagnostics.climate_ms = clock_milliseconds() - stage_start;

    stage_start = clock_milliseconds();
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_PREPARE_HYDROLOGY);
    hydrology_ok = world_gen_hydrology_build(context);
    {
        RiverGenerationDiagnostics hydrology_diagnostics = {0};
        river_generation_last_diagnostics(&hydrology_diagnostics);
        last_diagnostics.hydrology_bytes = (size_t)hydrology_diagnostics.transient_bytes;
        last_diagnostics.peak_bytes = last_diagnostics.context_bytes +
                                      last_diagnostics.hydrology_bytes;
    }
    stage_ms = clock_milliseconds() - stage_start;
    last_diagnostics.hydrology_ms = stage_ms;
    if (!hydrology_ok) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_HYDROLOGY);
        world_gen_diagnostics_finalize_failure(
            &last_diagnostics, context, WORLDGEN_ATTEMPT_PREPARE_HYDROLOGY,
            stage_ms, clock_milliseconds() - total_start);
        return 0;
    }

    stage_start = clock_milliseconds();
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_PREPARE_CLASSIFY);
    if (!world_gen_classify_final(context)) {
        stage_ms = clock_milliseconds() - stage_start;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_CLASSIFICATION);
        world_gen_diagnostics_finalize_failure(
            &last_diagnostics, context, WORLDGEN_ATTEMPT_PREPARE_CLASSIFY,
            stage_ms, clock_milliseconds() - total_start);
        return 0;
    }
    last_diagnostics.classification_ms = clock_milliseconds() - stage_start;

    last_diagnostics.physical_hash = context_physical_hash(context);
    world_gen_diagnostics_collect_counts(&last_diagnostics, context);
    worldgen_attempt_note_generated_counts(last_diagnostics.land_tiles,
                                            last_diagnostics.ocean_tiles,
                                            last_diagnostics.river_tiles);
    stage_start = clock_milliseconds();
    if (!stage_river_paths(context)) {
        world_gen_diagnostics_finalize_failure(
            &last_diagnostics, context, WORLDGEN_ATTEMPT_PREPARE_RIVER_PATHS,
            clock_milliseconds() - stage_start,
            clock_milliseconds() - total_start);
        return 0;
    }
    last_diagnostics.total_ms = clock_milliseconds() - total_start;
    return 1;
}

WorldGenContext *world_gen_prepare_for_dimensions(const WorldGenConfig *config,
                                                  int width, int height) {
    WorldGenConfig active = config ? *config : DEFAULT_WORLD_GEN_CONFIG;
    WorldGenContext *context;
    uint32_t seed = active.random_seed ? world_random_seed() : active.seed;
    int valid_dimensions = width > 0 && height > 0 &&
        width <= MAX_MAP_W && height <= MAX_MAP_H;
    int tile_count = valid_dimensions ? width * height : 0;
    int target_land = valid_dimensions
        ? world_gen_land_mask_target_tiles(active.ocean, tile_count) : 0;
    active.seed = seed;
    active.random_seed = 0;
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_PREPARE_CONTEXT);
    worldgen_attempt_note_target(width, height, target_land,
                                 valid_dimensions ? tile_count - target_land : 0);
    if (worldgen_fault_injection_should_fail(WORLDGEN_FAULT_PREPARE_ALLOCATION)) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARE_ALLOCATION_INJECTED);
        return NULL;
    }
    context = (WorldGenContext *)calloc(1, sizeof(*context));
    if (!context) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARE_CONTEXT_ALLOCATION);
        return NULL;
    }
    worldgen_attempt_note_context_allocation(0, 0, sizeof(*context));
    if (!world_gen_context_create(context, &active, width, height, seed) ||
        !world_gen_run_prepared(context)) {
        river_network_release_transient();
        world_gen_context_destroy(context);
        free(context);
        return NULL;
    }
    return context;
}

WorldGenContext *world_gen_prepare_with_config(const WorldGenConfig *config) {
    return world_gen_prepare_for_dimensions(config, MAP_W, MAP_H);
}

static int commit_physical_state(const WorldGenContext *context) {
    WorldPhysicalStateInput input;
    memset(&input, 0, sizeof(input));
    input.map_w = context->width;
    input.map_h = context->height;
    input.tile_count = context->tile_count;
    input.river_flow = context->river_flow;
    input.river_width = context->river_width;
    input.wind_direction16 = context->wind_direction16;
    input.wind_speed = context->wind_speed;
    input.soil_fertility = context->soil_fertility;
    input.river_order = context->river_order;
    input.river_flags = context->river_flags;
    return world_physical_state_commit_fields(&input);
}

static int prepared_staging_valid(const WorldGenContext *context,
                                  int width, int height) {
    const RiverPath *staged_paths;
    int required_paths;
    int copied_paths;
    if (!context || context->width != width || context->height != height) return 0;
    required_paths = context->staged_river_paths_required;
    copied_paths = context->staged_river_path_count;
    staged_paths = (const RiverPath *)context->staged_river_paths;
    return context->hydrology_token != 0 &&
           context->staged_river_token == context->hydrology_token &&
           context->staged_river_diagnostics_valid &&
           context->staged_river_diagnostics.legacy_paths_required == required_paths &&
           context->staged_river_diagnostics.legacy_paths_truncated == 0 &&
           river_path_count_valid(required_paths, width, height) &&
           copied_paths == required_paths &&
           river_presentation_state_can_adopt(staged_paths, copied_paths,
                                              width, height) &&
           river_paths_validate(staged_paths, copied_paths, width, height);
}

int world_gen_prepared_can_commit(const WorldGenContext *context,
                                  int width, int height) {
    WorldPhysicalStateInput input;
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_VALIDATE);
    if (!prepared_staging_valid(context, width, height)) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARED_VALIDATION);
        return 0;
    }
    memset(&input, 0, sizeof(input));
    input.map_w = context->width;
    input.map_h = context->height;
    input.tile_count = context->tile_count;
    input.river_flow = context->river_flow;
    input.river_width = context->river_width;
    input.wind_direction16 = context->wind_direction16;
    input.wind_speed = context->wind_speed;
    input.soil_fertility = context->soil_fertility;
    input.river_order = context->river_order;
    input.river_flags = context->river_flags;
    if (!world_physical_state_validate_fields(&input)) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARED_VALIDATION);
        return 0;
    }
    return 1;
}

static void commit_tiles(const WorldGenContext *context) {
    int y;
    int x;
    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            int index = world_gen_context_index(context, x, y);
            Tile *tile = &world[y][x];
            tile->geography = (Geography)context->geography[index];
            tile->climate = (Climate)context->climate[index];
            tile->ecology = (Ecology)context->ecology[index];
            tile->resource = (ResourceFeature)context->resource[index];
            tile->owner = -1;
            tile->province_id = -1;
            tile->region_id = -1;
            tile->elevation = context->elevation[index];
            tile->moisture = context->moisture[index];
            tile->temperature = context->temperature[index];
            tile->resource_variation = context->resource_variation[index];
            tile->river = (context->river_flags[index] & WORLD_GEN_RIVER_CHANNEL) != 0;
        }
    }
}

int world_gen_commit_prepared(WorldGenContext *context) {
    uint64_t commit_start;
    RiverPath *adopted_paths;
    int required_paths;
    int copied_paths;
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_COMMIT);
    if (!world_gen_prepared_can_commit(context, MAP_W, MAP_H)) return 0;
    required_paths = context->staged_river_paths_required;
    copied_paths = context->staged_river_path_count;
    adopted_paths = (RiverPath *)context->staged_river_paths;
    commit_start = clock_milliseconds();
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_COMMIT);
    if (!commit_physical_state(context)) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_COMMIT_PHYSICAL_STATE);
        return 0;
    }
    commit_tiles(context);
    river_presentation_state_adopt_prevalidated(&adopted_paths, copied_paths);
    river_generation_note_commit(&context->staged_river_diagnostics);
    context->staged_river_paths = adopted_paths;
    context->staged_river_path_count = 0;
    context->staged_river_paths_required = 0;
    context->staged_river_token = 0;
    last_diagnostics.river_segments_required = required_paths;
    last_diagnostics.river_segments_copied = river_path_count;
    terrain_stats_invalidate_cache();
    terrain_stats_rebuild_cache();
    world_seed_rng(context->phase_seed[WORLD_GEN_PHASE_DOWNSTREAM]);
    last_diagnostics.commit_ms = clock_milliseconds() - commit_start;
    last_diagnostics.total_ms += last_diagnostics.commit_ms;
    committed_diagnostics = last_diagnostics;
    last_config = context->config;
    last_config_valid = 1;
    committed_physical_revision = world_physical_state_revision();
    committed_diagnostics_valid = 1;
    return 1;
}

void world_gen_release_prepared(WorldGenContext *context) {
    if (!context) return;
    river_network_release_transient();
    world_gen_context_destroy(context);
    free(context);
}

const WorldGenDiagnostics *world_gen_last_diagnostics(void) {
    return &last_diagnostics;
}

int world_gen_last_committed_diagnostics(WorldGenDiagnostics *out) {
    if (!out || !committed_diagnostics_valid ||
        committed_physical_revision != world_physical_state_revision()) return 0;
    *out = committed_diagnostics;
    return 1;
}

int world_gen_last_committed_config(WorldGenConfig *out) {
    if (!out || !last_config_valid ||
        committed_physical_revision != world_physical_state_revision()) return 0;
    *out = last_config;
    return 1;
}

void generate_world_with_config(const WorldGenConfig *config) {
    WorldGenContext *context;
    int committed;
    worldgen_attempt_begin();
    context = world_gen_prepare_with_config(config);
    if (!context) {
        worldgen_attempt_finish(0);
        return;
    }
    committed = world_gen_commit_prepared(context);
    if (committed) worldgen_attempt_note_commit();
    world_gen_release_prepared(context);
    worldgen_attempt_finish(committed);
}
