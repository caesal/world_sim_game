#include "game/game_worldgen_diagnostics_probe.h"

#include "core/game_types.h"
#include "core/worldgen_attempt.h"
#include "core/worldgen_failure_notice.h"
#include "core/worldgen_fault_injection.h"
#include "world/rivers.h"
#include "world/world_gen.h"
#include "world/world_gen_context.h"
#include "world/world_physical_state.h"

#include <stdlib.h>
#include <string.h>

static WorldGenConfig diagnostics_config(unsigned int seed, int ocean) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = seed;
    config.random_seed = 0;
    config.ocean = ocean;
    config.continent = 61;
    config.relief = 57;
    config.moisture = 54;
    config.drought = 46;
    config.bias_wetland = 63;
    return config;
}

static int same_river_diagnostics(const RiverGenerationDiagnostics *a,
                                  const RiverGenerationDiagnostics *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

typedef struct {
    RiverPath *paths;
    int path_count;
    int physical_revision;
    int generated;
    uint64_t tile_hash;
    WorldGenDiagnostics world;
    RiverGenerationDiagnostics river;
} DiagnosticsLiveStamp;

static uint64_t live_tile_hash(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int x;
    int y;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            uint64_t value = (uint64_t)(unsigned int)world[y][x].geography |
                ((uint64_t)(unsigned int)world[y][x].climate << 8) |
                ((uint64_t)(unsigned int)world[y][x].elevation << 16) |
                ((uint64_t)(unsigned int)world[y][x].river << 32);
            hash ^= value;
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static int capture_live_stamp(DiagnosticsLiveStamp *stamp) {
    if (!stamp) return 0;
    memset(stamp, 0, sizeof(*stamp));
    stamp->paths = river_paths;
    stamp->path_count = river_path_count;
    stamp->physical_revision = world_physical_state_revision();
    stamp->generated = world_generated;
    stamp->tile_hash = live_tile_hash();
    return world_gen_last_committed_diagnostics(&stamp->world) &&
           river_generation_committed_diagnostics(&stamp->river);
}

static int live_stamp_unchanged(const DiagnosticsLiveStamp *stamp) {
    WorldGenDiagnostics world_after;
    RiverGenerationDiagnostics river_after;
    return stamp && stamp->paths == river_paths &&
        stamp->path_count == river_path_count &&
        stamp->physical_revision == world_physical_state_revision() &&
        stamp->generated == world_generated && stamp->tile_hash == live_tile_hash() &&
        world_gen_last_committed_diagnostics(&world_after) &&
        river_generation_committed_diagnostics(&river_after) &&
        memcmp(&stamp->world, &world_after, sizeof(world_after)) == 0 &&
        same_river_diagnostics(&stamp->river, &river_after);
}

static int failed_attempt_contract(FILE *file) {
    const int width = 96;
    const int height = 64;
    WorldGenConfig config = diagnostics_config(812347u, 44);
    WorldGenContext *context = NULL;
    WorldGenAttemptDiagnostics attempt;
    RiverGenerationDiagnostics committed_before;
    RiverGenerationDiagnostics committed_after;
    RiverGenerationDiagnostics attempted;
    WorldGenDiagnostics world_before;
    WorldGenDiagnostics world_after;
    uint32_t *saved_flow = NULL;
    RiverPath *live_paths = river_paths;
    int live_count = river_path_count;
    int live_revision = world_physical_state_revision();
    int before_ok = world_gen_last_committed_diagnostics(&world_before) &&
                    river_generation_committed_diagnostics(&committed_before);
    int failed = 0;
    int evidence_ok;
    int live_ok;
    int committed_ok;
    int reset_ok;
    int ok;

    worldgen_attempt_begin();
    if (before_ok) {
        worldgen_attempt_note_previous_world(world_generated, live_revision,
                                             world_before.physical_hash);
    }
    context = (WorldGenContext *)calloc(1, sizeof(*context));
    if (context && world_gen_context_create(context, &config, width, height,
                                            config.seed)) {
        saved_flow = context->river_flow;
        context->river_flow = NULL;
        failed = !world_gen_run_prepared(context);
        context->river_flow = saved_flow;
    }
    worldgen_attempt_note_elapsed(world_gen_last_diagnostics()->total_ms);
    worldgen_attempt_finish(0);
    worldgen_attempt_get(&attempt);
    river_generation_last_diagnostics(&attempted);
    if (context) world_gen_release_prepared(context);
    evidence_ok = failed &&
        attempt.last_failure_stage == WORLDGEN_ATTEMPT_PREPARE_HYDROLOGY &&
        attempt.last_failure_reason == WORLDGEN_FAILURE_HYDROLOGY &&
        attempt.target_width == width && attempt.target_height == height &&
        attempt.target_land_tiles + attempt.target_ocean_tiles == width * height &&
        attempt.actual_land_tiles + attempt.actual_ocean_tiles == width * height &&
        attempt.river_paths_required == 0 && attempt.river_paths_copied == 0 &&
        attempt.peak_allocation_bytes > 0;
    live_ok = live_paths == river_paths && live_count == river_path_count &&
              live_revision == world_physical_state_revision();
    committed_ok = before_ok &&
        world_gen_last_committed_diagnostics(&world_after) &&
        river_generation_committed_diagnostics(&committed_after) &&
        memcmp(&world_before, &world_after, sizeof(world_before)) == 0 &&
        same_river_diagnostics(&committed_before, &committed_after);
    reset_ok = attempted.transient_bytes == 0 && attempted.land_cells == 0 &&
               attempted.workspace_allocation_errors == 0 &&
               attempted.workspace_bytes_allocated == 0 &&
               attempted.legacy_paths_required == 0 &&
               attempted.legacy_paths_truncated == 0;
    ok = before_ok && evidence_ok && live_ok && committed_ok && reset_ok;
    fprintf(file,
            "case=worldgen_hydrology_failure stage=%s reason=%s target=%d/%d "
            "actual=%d/%d paths=%d/%d peak=%llu elapsed=%llu live=%d "
            "committed=%d attempt_reset=%d ok=%d\n",
            worldgen_attempt_stage_name(attempt.last_failure_stage),
            worldgen_failure_reason_name(attempt.last_failure_reason),
            attempt.target_land_tiles, attempt.target_ocean_tiles,
            attempt.actual_land_tiles, attempt.actual_ocean_tiles,
            attempt.river_paths_required, attempt.river_paths_copied,
            (unsigned long long)attempt.peak_allocation_bytes,
            (unsigned long long)attempt.elapsed_ms, live_ok, committed_ok,
            reset_ok, ok);
    return ok;
}

static int context_diagnostics_contract(FILE *file) {
    const int width = 96;
    const int height = 64;
    WorldGenConfig config_a = diagnostics_config(914771u, 30);
    WorldGenConfig config_b = diagnostics_config(914779u, 70);
    WorldGenContext *context_a;
    WorldGenContext *context_b;
    RiverGenerationDiagnostics last_before_null;
    RiverGenerationDiagnostics last_after_null;
    RiverGenerationDiagnostics committed;
    uint64_t token_b;
    int distinct;
    int null_rejected;
    int commit_a;
    int promote_a;
    int reject_b;
    int rejected_preserved;
    int commit_b;
    int promote_b;
    int ok;

    map_w = width;
    map_h = height;
    context_a = world_gen_prepare_for_dimensions(&config_a, width, height);
    context_b = world_gen_prepare_for_dimensions(&config_b, width, height);
    distinct = context_a && context_b &&
        context_a->staged_river_diagnostics_valid &&
        context_b->staged_river_diagnostics_valid &&
        context_a->staged_river_diagnostics.workspace_allocation_errors == 0 &&
        context_b->staged_river_diagnostics.workspace_allocation_errors == 0 &&
        context_a->staged_river_diagnostics.workspace_bytes_required ==
            context_a->staged_river_diagnostics.workspace_bytes_allocated &&
        context_b->staged_river_diagnostics.workspace_bytes_required ==
            context_b->staged_river_diagnostics.workspace_bytes_allocated &&
        context_a->staged_river_diagnostics.workspace_bytes_allocated > 0 &&
        context_b->staged_river_diagnostics.workspace_bytes_allocated > 0 &&
        !same_river_diagnostics(&context_a->staged_river_diagnostics,
                                &context_b->staged_river_diagnostics);
    river_generation_last_diagnostics(&last_before_null);
    null_rejected = river_network_copy_legacy_paths(NULL, 1) == 0;
    river_generation_last_diagnostics(&last_after_null);
    null_rejected = null_rejected &&
        same_river_diagnostics(&last_before_null, &last_after_null);
    commit_a = context_a && world_gen_commit_prepared(context_a);
    promote_a = commit_a && river_generation_committed_diagnostics(&committed) &&
        same_river_diagnostics(&committed,
                               &context_a->staged_river_diagnostics);
    token_b = context_b ? context_b->staged_river_token : 0;
    if (context_b) context_b->staged_river_token ^= UINT64_C(1);
    reject_b = context_b && !world_gen_commit_prepared(context_b);
    rejected_preserved = reject_b &&
        river_generation_committed_diagnostics(&committed) &&
        same_river_diagnostics(&committed,
                               &context_a->staged_river_diagnostics);
    if (context_b) context_b->staged_river_token = token_b;
    commit_b = context_b && world_gen_commit_prepared(context_b);
    promote_b = commit_b && river_generation_committed_diagnostics(&committed) &&
        same_river_diagnostics(&committed,
                               &context_b->staged_river_diagnostics);
    ok = distinct && null_rejected && commit_a && promote_a && reject_b &&
         rejected_preserved && commit_b && promote_b;
    fprintf(file,
            "case=worldgen_context_diagnostics distinct=%d null_rejected=%d "
            "commit_a=%d promote_a=%d reject_b=%d preserved=%d "
            "commit_b=%d promote_b=%d paths_a=%d paths_b=%d ok=%d\n",
            distinct, null_rejected, commit_a, promote_a, reject_b,
            rejected_preserved, commit_b, promote_b,
            context_a ? context_a->staged_river_paths_required : -1,
            context_b ? context_b->staged_river_paths_required : -1, ok);
    if (context_a) world_gen_release_prepared(context_a);
    if (context_b) world_gen_release_prepared(context_b);
    return ok;
}

static WorldGenConfig allocation_config(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = 3300337u;
    config.random_seed = 0;
    config.ocean = 47;
    config.continent = 58;
    config.relief = 61;
    config.moisture = 43;
    config.drought = 52;
    config.vegetation = 57;
    config.bias_forest = 62;
    config.bias_desert = 41;
    config.bias_mountain = 66;
    config.bias_wetland = 39;
    return config;
}

static int allocation_failure_case(FILE *file, const DiagnosticsLiveStamp *live,
                                   const char *label, WorldGenFaultPoint point,
                                   int first_call,
                                   WorldGenAttemptStage expected_stage,
                                   WorldGenFailureReason expected_reason) {
    const int width = 576;
    const int height = 400;
    WorldGenConfig config = allocation_config();
    WorldGenContext *context;
    WorldGenAttemptDiagnostics attempt;
    WorldGenDiagnostics generation;
    RiverGenerationDiagnostics hydrology;
    char text_en[256];
    char text_zh[256];
    int triggered;
    int diagnostic_ok;
    int localized;
    int retained;
    int ok;
    uint64_t workspace_tile_count = (uint64_t)width * (uint64_t)height;

    memset(&hydrology, 0, sizeof(hydrology));
    worldgen_fault_injection_clear();
    worldgen_attempt_begin();
    worldgen_attempt_note_previous_world(
        live->generated, live->physical_revision, live->world.physical_hash);
    worldgen_fault_injection_arm(point, first_call, 1);
    context = world_gen_prepare_for_dimensions(&config, width, height);
    triggered = worldgen_fault_injection_was_triggered(point);
    if (context) world_gen_release_prepared(context);
    worldgen_attempt_finish(0);
    worldgen_attempt_get(&attempt);
    generation = *world_gen_last_diagnostics();
    river_generation_last_diagnostics(&hydrology);
    localized = worldgen_failure_notice_format(
        expected_reason, text_en, sizeof(text_en), text_zh, sizeof(text_zh)) &&
        strstr(text_en, worldgen_failure_reason_text_en(expected_reason)) != NULL &&
        strstr(text_zh, worldgen_failure_reason_text_zh(expected_reason)) != NULL;
    diagnostic_ok = attempt.last_failure_stage == expected_stage &&
        attempt.last_failure_reason == expected_reason &&
        attempt.target_width == width && attempt.target_height == height &&
        attempt.target_land_tiles + attempt.target_ocean_tiles == width * height &&
        attempt.actual_land_tiles + attempt.actual_ocean_tiles == width * height &&
        attempt.peak_allocation_bytes > 0 &&
        worldgen_fault_injection_call_count(point) == first_call &&
        ((expected_reason == WORLDGEN_FAILURE_RIVER_WORKSPACE_ALLOCATION &&
          hydrology.workspace_allocation_errors == 1 &&
          hydrology.workspace_allocation_failure_field == first_call &&
          hydrology.workspace_bytes_required == workspace_tile_count * 37u &&
          hydrology.workspace_bytes_allocated == workspace_tile_count * 24u &&
          hydrology.transient_bytes == hydrology.workspace_bytes_allocated) ||
         (expected_reason == WORLDGEN_FAILURE_RIVER_DISTRIBUTARY_ALLOCATION &&
          hydrology.distributary_allocation_errors == 1) ||
         (expected_reason == WORLDGEN_FAILURE_RIVER_SEGMENT_ALLOCATION &&
          hydrology.segment_allocation_errors == 1) ||
         (expected_reason == WORLDGEN_FAILURE_RIVER_PATH_ALLOCATION &&
          attempt.river_paths_required > 0 && attempt.river_paths_copied == 0) ||
         (expected_reason == WORLDGEN_FAILURE_RIVER_PATH_COPY &&
          attempt.river_paths_required > 0 && attempt.river_paths_copied == 0 &&
          attempt.river_path_capacity == attempt.river_paths_required &&
          attempt.staged_allocation_bytes > 0 &&
          generation.staged_path_bytes == attempt.staged_allocation_bytes &&
          generation.peak_bytes == generation.context_bytes +
              generation.hydrology_bytes + generation.staged_path_bytes &&
          attempt.peak_allocation_bytes == generation.peak_bytes));
    retained = live_stamp_unchanged(live);
    ok = !context && triggered && diagnostic_ok && localized && retained;
    fprintf(file,
            "case=worldgen_allocation_fault label=%s stage=%s reason=%s "
            "triggered=%d calls=%d target=%d/%d actual=%d/%d paths=%d/%d "
            "storage_errors=%d/%d/%d workspace=%llu/%llu field=%d "
            "peak=%llu elapsed=%llu localized=%d "
            "prior_retained=%d ok=%d\n",
            label, worldgen_attempt_stage_name(attempt.last_failure_stage),
            worldgen_failure_reason_name(attempt.last_failure_reason), triggered,
            worldgen_fault_injection_call_count(point), attempt.target_land_tiles,
            attempt.target_ocean_tiles, attempt.actual_land_tiles,
            attempt.actual_ocean_tiles, attempt.river_paths_required,
            attempt.river_paths_copied, hydrology.workspace_allocation_errors,
            hydrology.distributary_allocation_errors,
            hydrology.segment_allocation_errors,
            (unsigned long long)hydrology.workspace_bytes_allocated,
            (unsigned long long)hydrology.workspace_bytes_required,
            hydrology.workspace_allocation_failure_field,
            (unsigned long long)attempt.peak_allocation_bytes,
            (unsigned long long)attempt.elapsed_ms, localized, retained, ok);
    worldgen_fault_injection_clear();
    return ok;
}

static int allocation_failure_contract(FILE *file) {
    DiagnosticsLiveStamp live;
    int ready = capture_live_stamp(&live);
    int workspace_ok = ready && allocation_failure_case(
        file, &live, "workspace", WORLDGEN_FAULT_RIVER_WORKSPACE_ALLOCATION, 7,
        WORLDGEN_ATTEMPT_PREPARE_HYDROLOGY,
        WORLDGEN_FAILURE_RIVER_WORKSPACE_ALLOCATION);
    int distributary_ok = ready && allocation_failure_case(
        file, &live, "distributary", WORLDGEN_FAULT_RIVER_DISTRIBUTARY_ALLOCATION,
        1, WORLDGEN_ATTEMPT_PREPARE_HYDROLOGY,
        WORLDGEN_FAILURE_RIVER_DISTRIBUTARY_ALLOCATION);
    int segment_ok = ready && allocation_failure_case(
        file, &live, "segment", WORLDGEN_FAULT_RIVER_SEGMENT_ALLOCATION,
        1, WORLDGEN_ATTEMPT_PREPARE_HYDROLOGY,
        WORLDGEN_FAILURE_RIVER_SEGMENT_ALLOCATION);
    int path_ok = ready && allocation_failure_case(
        file, &live, "path", WORLDGEN_FAULT_RIVER_PATH_ALLOCATION,
        1, WORLDGEN_ATTEMPT_PREPARE_RIVER_PATHS,
        WORLDGEN_FAILURE_RIVER_PATH_ALLOCATION);
    int copy_ok = ready && allocation_failure_case(
        file, &live, "path-copy", WORLDGEN_FAULT_RIVER_PATH_COPY,
        1, WORLDGEN_ATTEMPT_PREPARE_RIVER_PATHS,
        WORLDGEN_FAILURE_RIVER_PATH_COPY);
    int ok = ready && workspace_ok && distributary_ok && segment_ok &&
             path_ok && copy_ok;
    fprintf(file,
            "case=worldgen_allocation_atomicity ready=%d workspace=%d distributary=%d "
            "segment=%d path=%d copy=%d revision=%d paths=%d ok=%d\n",
            ready, workspace_ok, distributary_ok, segment_ok, path_ok, copy_ok,
            live.physical_revision, live.path_count, ok);
    return ok;
}

static int context_prefix_failure_contract(FILE *file) {
    const int width = 96;
    const int height = 64;
    const int first_call = 25;
    const int expected_field = 24;
    const uint64_t expected_prefix = (uint64_t)width * height * 35u;
    WorldGenConfig config = diagnostics_config(441923u, 45);
    DiagnosticsLiveStamp live;
    WorldGenAttemptDiagnostics attempt;
    WorldGenContext *context;
    int ready = capture_live_stamp(&live);
    int retained;
    int ok;
    worldgen_fault_injection_clear();
    worldgen_attempt_begin();
    if (ready) worldgen_attempt_note_previous_world(
        live.generated, live.physical_revision, live.world.physical_hash);
    worldgen_fault_injection_arm(
        WORLDGEN_FAULT_PREPARE_ALLOCATION, first_call, 1);
    context = world_gen_prepare_for_dimensions(&config, width, height);
    if (context) world_gen_release_prepared(context);
    worldgen_attempt_finish(0);
    worldgen_attempt_get(&attempt);
    retained = ready && live_stamp_unchanged(&live);
    ok = ready && !context && retained &&
        worldgen_fault_injection_was_triggered(
            WORLDGEN_FAULT_PREPARE_ALLOCATION) &&
        worldgen_fault_injection_call_count(
            WORLDGEN_FAULT_PREPARE_ALLOCATION) == first_call &&
        attempt.last_failure_stage == WORLDGEN_ATTEMPT_PREPARE_CONTEXT &&
        attempt.last_failure_reason == WORLDGEN_FAILURE_PREPARE_ALLOCATION_INJECTED &&
        attempt.context_allocation_failure_field == expected_field &&
        attempt.context_allocated_bytes == expected_prefix &&
        attempt.peak_allocation_bytes ==
            expected_prefix + sizeof(WorldGenContext);
    fprintf(file,
            "case=worldgen_context_prefix_fault calls=%d field=%d "
            "prefix=%llu expected=%llu peak=%llu expected_peak=%llu "
            "retained=%d ok=%d\n",
            worldgen_fault_injection_call_count(
                WORLDGEN_FAULT_PREPARE_ALLOCATION),
            attempt.context_allocation_failure_field,
            (unsigned long long)attempt.context_allocated_bytes,
            (unsigned long long)expected_prefix,
            (unsigned long long)attempt.peak_allocation_bytes,
            (unsigned long long)(expected_prefix + sizeof(WorldGenContext)),
            retained, ok);
    worldgen_fault_injection_clear();
    return ok;
}

static int early_failure_finalization_contract(FILE *file) {
    const int width = 96;
    const int height = 64;
    WorldGenConfig config = diagnostics_config(771923u, 45);
    WorldGenContext context;
    WorldGenAttemptDiagnostics attempt;
    DiagnosticsLiveStamp live;
    uint8_t *coastal_hint;
    int ready = capture_live_stamp(&live);
    int created;
    int failed = 0;
    int retained;
    int ok;

    memset(&context, 0, sizeof(context));
    worldgen_attempt_begin();
    created = world_gen_context_create(&context, &config, width, height,
                                       config.seed);
    coastal_hint = context.coastal_lowland_hint;
    if (created) {
        context.coastal_lowland_hint = NULL;
        failed = !world_gen_run_prepared(&context);
        context.coastal_lowland_hint = coastal_hint;
    }
    worldgen_attempt_finish(0);
    worldgen_attempt_get(&attempt);
    world_gen_context_destroy(&context);
    retained = ready && live_stamp_unchanged(&live);
    ok = ready && created && failed && retained &&
        attempt.last_failure_stage == WORLDGEN_ATTEMPT_PREPARE_ELEVATION &&
        attempt.last_failure_reason == WORLDGEN_FAILURE_LAND_MASK_INVALID_TARGET &&
        attempt.target_width == width && attempt.target_height == height &&
        attempt.target_land_tiles + attempt.target_ocean_tiles == width * height &&
        attempt.actual_land_tiles + attempt.actual_ocean_tiles == width * height &&
        attempt.peak_allocation_bytes > 0;
    fprintf(file,
            "case=worldgen_early_failure_finalization stage=%s reason=%s "
            "target=%d/%d actual=%d/%d peak=%llu elapsed=%llu retained=%d ok=%d\n",
            worldgen_attempt_stage_name(attempt.last_failure_stage),
            worldgen_failure_reason_name(attempt.last_failure_reason),
            attempt.target_land_tiles, attempt.target_ocean_tiles,
            attempt.actual_land_tiles, attempt.actual_ocean_tiles,
            (unsigned long long)attempt.peak_allocation_bytes,
            (unsigned long long)attempt.elapsed_ms, retained, ok);
    return ok;
}

int game_worldgen_diagnostics_probe_run(FILE *file) {
    int failure_ok = failed_attempt_contract(file);
    int context_ok = context_diagnostics_contract(file);
    int allocation_ok = allocation_failure_contract(file);
    int context_prefix_ok = context_prefix_failure_contract(file);
    int early_ok = early_failure_finalization_contract(file);
    return failure_ok && context_ok && allocation_ok && context_prefix_ok &&
           early_ok;
}
