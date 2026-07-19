#include "game/game_worldgen_coast_threshold_probe.h"
#include "game/game_worldgen_coast_semantic_fixture.h"
#include "game/game_worldgen_river_e2e_probe.h"

#include "core/constants.h"
#include "core/worldgen_attempt.h"
#include "world/river_path_validation.h"
#include "world/river_presentation_state.h"
#include "world/rivers.h"
#include "world/world_gen.h"
#include "world/world_gen_context.h"
#include "world/world_gen_land_mask.h"
#include "world/world_physical_state.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef struct {
    uint64_t mask_hash;
    uint64_t physical_hash;
    int target_land;
    int actual_land;
    int actual_ocean;
    int river_required;
    int river_copied;
    int river_limit;
    int ok;
} CoastThresholdCaseResult;

enum { LEGACY_DISTRIBUTARY_LIMIT = 384 };

static WorldGenConfig probe_config(unsigned int seed, int ocean, int variant);
static int semantic_metrics_clear(const WorldGenLandMaskDiagnostics *metrics);

static int expected_land_tiles(int ocean, int tile_count) {
    int clamped = ocean < 0 ? 0 : ocean > 100 ? 100 : ocean;
    int64_t numerator = (int64_t)tile_count * (10000 - 72 * clamped);
    return (int)((numerator + 5000) / 10000);
}

static int target_precision_contract(FILE *file) {
    static const struct {
        int ocean;
        int expected_land;
    } fixed[] = {
        {0, 921600}, {10, 855245}, {20, 788890}, {30, 722534},
        {40, 656179}, {44, 629637}, {45, 623002}, {46, 616366},
        {47, 609731}, {50, 589824}, {75, 423936}, {90, 324403},
        {99, 264684}, {100, 258048}
    };
    const int tile_count = MAX_MAP_W * MAX_MAP_H;
    int fixed_ok = 1;
    int adjacent_ok = 1;
    int previous = tile_count + 1;
    int ocean;
    int index;
    for (index = 0; index < (int)(sizeof(fixed) / sizeof(fixed[0])); index++) {
        int production = world_gen_land_mask_target_tiles(fixed[index].ocean,
                                                          tile_count);
        fixed_ok &= production == fixed[index].expected_land &&
                    production == expected_land_tiles(fixed[index].ocean,
                                                      tile_count);
    }
    for (ocean = 0; ocean <= 100; ocean++) {
        int production = world_gen_land_mask_target_tiles(ocean, tile_count);
        adjacent_ok &= production == expected_land_tiles(ocean, tile_count);
        if (ocean > 0) adjacent_ok &= production < previous;
        previous = production;
    }
    fprintf(file,
            "case=coast_target_precision fixed=%d adjacent=%d ocean50_land=%d "
            "ocean50_ocean=%d ok=%d\n",
            fixed_ok, adjacent_ok,
            world_gen_land_mask_target_tiles(50, tile_count),
            tile_count - world_gen_land_mask_target_tiles(50, tile_count),
            fixed_ok && adjacent_ok);
    return fixed_ok && adjacent_ok;
}

typedef struct {
    uint64_t hash;
    int target;
    int land;
    int full_checker_cells;
    int full_diagonal_only_cells;
    int normalized;
    int ok;
} FlatPlateauResult;

static FlatPlateauResult run_flat_plateau_once(unsigned int seed) {
    const int width = 64;
    const int height = 48;
    WorldGenConfig config = probe_config(seed, 45, 0);
    WorldGenContext *context = (WorldGenContext *)calloc(1, sizeof(*context));
    FlatPlateauResult result = {0};
    int index;
    if (!context || !world_gen_context_create(context, &config, width, height, seed)) {
        free(context);
        return result;
    }
    for (index = 0; index < context->tile_count; index++) context->elevation[index] = 50;
    if (world_gen_land_mask_build(context)) {
        const WorldGenLandMaskDiagnostics *metrics =
            world_gen_land_mask_diagnostics(context);
        result.hash = metrics->mask_hash;
        result.target = expected_land_tiles(config.ocean, context->tile_count);
        result.land = metrics->final_land_tiles;
        result.normalized = 1;
        for (index = 0; index < context->tile_count; index++) {
            int state = context->land_mask[index] != 0;
            int x = index % width;
            int y = index / width;
            int cardinal = 0;
            int diagonal = 0;
            int dx;
            int dy;
            result.normalized &= context->elevation[index] == (state ? 51 : 49);
            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    int next;
                    if ((dx == 0 && dy == 0) || x + dx < 0 || x + dx >= width ||
                        y + dy < 0 || y + dy >= height) continue;
                    next = (y + dy) * width + x + dx;
                    if ((context->land_mask[next] != 0) != state) continue;
                    if (dx == 0 || dy == 0) cardinal++;
                    else diagonal++;
                }
            }
            if (cardinal == 0 && diagonal > 0) result.full_diagonal_only_cells++;
            if (x + 1 < width && y + 1 < height) {
                int a = state;
                int b = context->land_mask[index + 1] != 0;
                int c = context->land_mask[index + width] != 0;
                int d = context->land_mask[index + width + 1] != 0;
                if (a == d && b == c && a != b) result.full_checker_cells++;
            }
        }
        result.ok = result.land == result.target && result.normalized &&
                    metrics->threshold_candidates == context->tile_count &&
                    metrics->threshold_selected == result.target &&
                    metrics->coastal_lowland_tiles == result.target &&
                    metrics->land_components == 1 && metrics->water_components == 1 &&
                    semantic_metrics_clear(metrics) && result.full_checker_cells == 0 &&
                    result.full_diagonal_only_cells == 0;
    }
    world_gen_context_destroy(context);
    free(context);
    return result;
}

static int flat_plateau_contract(FILE *file) {
    FlatPlateauResult first = run_flat_plateau_once(611937u);
    FlatPlateauResult repeat = run_flat_plateau_once(611937u);
    int deterministic = first.ok && repeat.ok && first.hash == repeat.hash &&
                        first.target == repeat.target && first.land == repeat.land;
    fprintf(file,
            "case=coast_flat_plateau target=%d land=%d checker=%d diagonal_only=%d "
            "normalized=%d hash=%016llx/%016llx deterministic=%d ok=%d\n",
            first.target, first.land, first.full_checker_cells,
            first.full_diagonal_only_cells, first.normalized,
            (unsigned long long)first.hash, (unsigned long long)repeat.hash,
            deterministic, deterministic);
    return deterministic;
}

static WorldGenConfig probe_config(unsigned int seed, int ocean, int variant) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = seed;
    config.random_seed = 0;
    config.ocean = ocean;
    if (variant == 0) {
        config.continent = 58;
        config.relief = 61;
        config.moisture = 43;
        config.drought = 52;
        config.vegetation = 57;
        config.bias_forest = 62;
        config.bias_desert = 41;
        config.bias_mountain = 66;
        config.bias_wetland = 39;
    } else {
        config.continent = 43;
        config.relief = 83;
        config.moisture = 85;
        config.drought = 16;
        config.vegetation = 70;
        config.bias_forest = 62;
        config.bias_desert = 72;
        config.bias_mountain = 46;
        config.bias_wetland = 64;
    }
    return config;
}

static int semantic_metrics_clear(const WorldGenLandMaskDiagnostics *metrics) {
    return metrics && metrics->failure == WORLD_GEN_LAND_MASK_OK &&
           metrics->target_drift == 0 && metrics->lattice_cells == 0 &&
           metrics->comb_cells == 0 && metrics->mesh_cells == 0 &&
           metrics->tendril_cells == 0 && metrics->topology_errors == 0;
}

static int deterministic_result_equal(const CoastThresholdCaseResult *first,
                                      const CoastThresholdCaseResult *second) {
    return first && second && first->ok && second->ok &&
           first->mask_hash == second->mask_hash &&
           first->physical_hash == second->physical_hash &&
           first->target_land == second->target_land &&
           first->actual_land == second->actual_land &&
           first->actual_ocean == second->actual_ocean &&
           first->river_required == second->river_required &&
           first->river_copied == second->river_copied &&
           first->river_limit == second->river_limit;
}

static int run_case(FILE *file, const char *label, const WorldGenConfig *config,
                    CoastThresholdCaseResult *out_result) {
    WorldGenContext *context = NULL;
    const WorldGenLandMaskDiagnostics *land_metrics = NULL;
    const WorldGenDiagnostics *generation = NULL;
    WorldGenAttemptDiagnostics attempt = {0};
    RiverGenerationDiagnostics river_metrics = {0};
    CoastThresholdCaseResult result = {0};
    int expected = expected_land_tiles(config->ocean, MAX_MAP_W * MAX_MAP_H);
    int context_created = 0;
    int prepared = 0;
    int path_storage_ok = 0;
    int legacy_limit_case = config->seed == 3300337u && config->continent != 43 &&
                            config->ocean == 44;
    int index;

    worldgen_attempt_begin();
    context = (WorldGenContext *)calloc(1, sizeof(*context));
    if (!context) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREPARE_CONTEXT_ALLOCATION);
    } else {
        context_created = world_gen_context_create(
            context, config, MAX_MAP_W, MAX_MAP_H, config->seed);
        if (context_created) prepared = world_gen_run_prepared(context);
    }

    generation = world_gen_last_diagnostics();
    if (context_created) {
        land_metrics = world_gen_land_mask_diagnostics(context);
        river_generation_last_diagnostics(&river_metrics);
        result.target_land = land_metrics ? land_metrics->target_land_tiles : -1;
        result.mask_hash = land_metrics ? land_metrics->mask_hash : 0;
        result.physical_hash = generation ? generation->physical_hash : 0;
        result.river_required = context->staged_river_paths_required;
        result.river_copied = context->staged_river_path_count;
        result.river_limit = river_path_count_limit(context->width, context->height);
        for (index = 0; index < context->tile_count; index++) {
            if (context->land_mask[index]) result.actual_land++;
            else result.actual_ocean++;
        }
        path_storage_ok = result.river_limit >= result.river_required &&
                          river_path_count_valid(result.river_required,
                                                 context->width, context->height) &&
                          result.river_required == result.river_copied &&
                          result.river_required == river_metrics.legacy_paths_required &&
                          river_metrics.legacy_paths_truncated == 0 &&
                          (result.river_copied == 0 || context->staged_river_paths) &&
                          context->staged_river_token == context->hydrology_token &&
                          river_paths_validate(
                              (const RiverPath *)context->staged_river_paths,
                              result.river_copied, context->width, context->height);
    } else {
        result.target_land = -1;
        result.river_required = generation ? generation->river_segments_required : -1;
        result.river_copied = -1;
        result.river_limit = river_path_count_limit(MAX_MAP_W, MAX_MAP_H);
    }

    worldgen_attempt_finish(prepared);
    worldgen_attempt_get(&attempt);
    result.ok = prepared && land_metrics && generation &&
                result.target_land == expected &&
                land_metrics->final_land_tiles == expected &&
                result.actual_land == expected &&
                result.actual_ocean == MAX_MAP_W * MAX_MAP_H - expected &&
                semantic_metrics_clear(land_metrics) &&
                result.mask_hash != 0 && result.physical_hash != 0 &&
                generation->land_tiles == result.actual_land &&
                generation->ocean_tiles == result.actual_ocean &&
                generation->river_segments_required == result.river_required &&
                attempt.river_paths_required == result.river_required &&
                attempt.river_paths_copied == result.river_copied &&
                attempt.river_path_capacity == result.river_required &&
                path_storage_ok && river_metrics.distributary_allocation_errors == 0 &&
                river_metrics.segment_allocation_errors == 0 &&
                (!legacy_limit_case ||
                 river_metrics.distributaries > LEGACY_DISTRIBUTARY_LIMIT);

    fprintf(file,
            "case=coast_threshold label=%s seed=%u variant=%d ocean=%d "
            "target=%d/%d final=%d land=%d ocean_tiles=%d drift=%d "
            "failure=%d lattice=%d comb=%d mesh=%d tendril=%d topology=%d "
            "mask_hash=%016llx physical_hash=%016llx "
            "river_required=%d copied=%d allocated_capacity=%d map_limit=%d "
            "river_diag_required=%d truncated=%d stage=%s reason=%s "
            "hydrology=land:%d/topo:%d invalid:%d cycle:%d dead:%d "
            "crossing:%d/%d lake_final:%d flow:%d width:%d order:%d "
            "storage_errors:%d/%d distributaries:%d "
            "duplicate:%d ok=%d\n",
            label, config->seed, config->continent == 43 ? 1 : 0, config->ocean,
            result.target_land, expected,
            land_metrics ? land_metrics->final_land_tiles : -1,
            result.actual_land, result.actual_ocean,
            land_metrics ? land_metrics->target_drift : -1,
            land_metrics ? (int)land_metrics->failure : -1,
            land_metrics ? land_metrics->lattice_cells : -1,
            land_metrics ? land_metrics->comb_cells : -1,
            land_metrics ? land_metrics->mesh_cells : -1,
            land_metrics ? land_metrics->tendril_cells : -1,
            land_metrics ? land_metrics->topology_errors : -1,
            (unsigned long long)result.mask_hash,
            (unsigned long long)result.physical_hash,
            result.river_required, result.river_copied,
            attempt.river_path_capacity, result.river_limit,
            river_metrics.legacy_paths_required,
            river_metrics.legacy_paths_truncated,
            worldgen_attempt_stage_name(attempt.last_failure_stage),
            worldgen_failure_reason_name(attempt.last_failure_reason),
            river_metrics.land_cells, river_metrics.topological_cells,
            river_metrics.invalid_receivers, river_metrics.cycle_errors,
            river_metrics.inland_dead_ends, river_metrics.crossing_errors,
            river_metrics.crossing_repairs,
            river_metrics.lake_final_invalid_components,
            river_metrics.flow_conservation_errors,
            river_metrics.width_regressions, river_metrics.order_errors,
            river_metrics.distributary_allocation_errors,
            river_metrics.segment_allocation_errors,
            river_metrics.distributaries,
            river_metrics.duplicate_edges, result.ok);
    fflush(file);
    if (out_result) *out_result = result;
    world_gen_release_prepared(context);
    return result.ok;
}

static int prepare_output_path(char *path, size_t path_size) {
    const char *root =
        "build/validation/coast_low_ocean_river_lod_20260718/02_focused";
    char attempt[MAX_PATH];
    int number;
    if (!path || path_size == 0) return 0;
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA("build/validation/coast_low_ocean_river_lod_20260718", NULL);
    CreateDirectoryA(root, NULL);
    for (number = 1; number <= 999; number++) {
        snprintf(attempt, sizeof(attempt), "%s/attempt_%03d", root, number);
        if (GetFileAttributesA(attempt) != INVALID_FILE_ATTRIBUTES) continue;
        if (!CreateDirectoryA(attempt, NULL)) return 0;
        snprintf(path, path_size, "%s/worldgen_coast_threshold_probe.txt", attempt);
        return 1;
    }
    return 0;
}

int run_worldgen_coast_threshold_probe(void) {
    static const int broad_oceans[] = {
        0, 10, 20, 30, 40, 44, 45, 46, 47, 50, 75, 90, 99, 100
    };
    static const int boundary_oceans[] = {44, 45, 46, 47};
    static const unsigned int boundary_seeds[] = {3300337u, 2026058350u, 731905u};
    char path[MAX_PATH + 64];
    CoastThresholdCaseResult deterministic_first = {0};
    CoastThresholdCaseResult deterministic_repeat = {0};
    RiverPath *initial_paths = river_paths;
    int initial_path_count = river_path_count;
    int initial_physical_revision = world_physical_state_revision();
    FILE *file;
    int ok = 1;
    int index;
    int seed_index;
    int variant;

    if (!prepare_output_path(path, sizeof(path))) return 1;
    file = fopen(path, "w");
    if (!file) return 1;
    fprintf(file, "probe=worldgen_coast_threshold map=%dx%d begin=1\n",
            MAX_MAP_W, MAX_MAP_H);
    ok &= target_precision_contract(file);
    ok &= flat_plateau_contract(file);
    ok &= game_worldgen_coast_semantic_fixture_run(file);
    fflush(file);

    for (index = 0; index < (int)(sizeof(broad_oceans) / sizeof(broad_oceans[0]));
         index++) {
        char label[64];
        WorldGenConfig config = probe_config(3300337u, broad_oceans[index], 0);
        snprintf(label, sizeof(label), "broad_ocean_%d", broad_oceans[index]);
        ok &= run_case(file, label, &config,
                       broad_oceans[index] == 45 ? &deterministic_first : NULL);
    }

    for (seed_index = 0;
         seed_index < (int)(sizeof(boundary_seeds) / sizeof(boundary_seeds[0]));
         seed_index++) {
        for (variant = 0; variant < 2; variant++) {
            for (index = 0;
                 index < (int)(sizeof(boundary_oceans) / sizeof(boundary_oceans[0]));
                 index++) {
                char label[96];
                WorldGenConfig config = probe_config(
                    boundary_seeds[seed_index], boundary_oceans[index], variant);
                snprintf(label, sizeof(label), "boundary_seed_%u_set_%d_ocean_%d",
                         boundary_seeds[seed_index], variant, boundary_oceans[index]);
                ok &= run_case(file, label, &config, NULL);
            }
        }
    }

    {
        WorldGenConfig config = probe_config(3300337u, 45, 0);
        int repeat_ok = run_case(file, "determinism_repeat_ocean_45", &config,
                                 &deterministic_repeat);
        int deterministic = repeat_ok && deterministic_result_equal(
            &deterministic_first, &deterministic_repeat);
        fprintf(file,
                "case=coast_threshold_determinism mask=%016llx/%016llx "
                "physical=%016llx/%016llx paths=%d/%d ok=%d\n",
                (unsigned long long)deterministic_first.mask_hash,
                (unsigned long long)deterministic_repeat.mask_hash,
                (unsigned long long)deterministic_first.physical_hash,
                (unsigned long long)deterministic_repeat.physical_hash,
                deterministic_first.river_required,
                deterministic_repeat.river_required, deterministic);
        ok &= deterministic;
    }

    {
        int committed_state_unchanged =
            initial_paths == river_paths && initial_path_count == river_path_count &&
            initial_physical_revision == world_physical_state_revision();
        fprintf(file,
                "case=coast_threshold_no_commit physical_revision=%d/%d "
                "path_count=%d/%d pointer_same=%d ok=%d\n",
                initial_physical_revision, world_physical_state_revision(),
                initial_path_count, river_path_count, initial_paths == river_paths,
                committed_state_unchanged);
        ok &= committed_state_unchanged;
    }
    ok &= game_worldgen_river_e2e_probe_run(file);
    fprintf(file, "overall_ok=%d\n", ok);
    fclose(file);
    printf("worldgen coast threshold probe: %s\n", path);
    return ok ? 0 : 1;
}
