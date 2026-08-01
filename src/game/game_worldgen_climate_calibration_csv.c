#include "game/game_worldgen_climate_calibration_csv.h"
#include <ctype.h>
#include <io.h>
#include <string.h>
#include <windows.h>
#define BASE_FIELDS(X) \
    X(F, schema_version, identity.schema_version) \
    X(S, run_id, identity.run_id) \
    X(S, source_head, identity.source_head) \
    X(S, source_manifest_hash, identity.source_manifest_hash) \
    X(S, executable_hash, identity.executable_hash) \
    X(S, scripts_manifest_hash, identity.scripts_manifest_hash) \
    X(S, config_manifest_hash, identity.config_manifest_hash) \
    X(U, seed, spec.seed) \
    X(S, seed_kind, spec.seed_kind) \
    X(I, shard_config_start, spec.shard_config_start) \
    X(I, shard_config_count, spec.shard_config_count) \
    X(I, shard_row_index, spec.shard_row_index) \
    X(I, config_index, spec.config_index) \
    X(I, ocean, spec.ocean) X(I, continent, spec.continent) \
    X(I, relief, spec.relief) X(I, vegetation, spec.vegetation) \
    X(I, bias_forest, spec.bias_forest) X(I, bias_desert, spec.bias_desert) \
    X(I, bias_mountain, spec.bias_mountain) X(I, bias_wetland, spec.bias_wetland) \
    X(I, moisture, spec.moisture) X(I, drought, spec.drought) \
    X(I, random_seed, spec.random_seed) \
    X(U, config_multiplicity, spec.config_multiplicity) \
    X(I, map_size, spec.map_size) \
    X(S, map_size_name, spec.map_size_name) \
    X(I, width, spec.width) \
    X(I, height, spec.height) \
    X(I, success, success) \
    X(S, failure_stage, failure_stage) \
    X(S, failure_reason, failure_reason) \
    X(I, world_diagnostics_valid, world_diagnostics_valid) \
    X(I, land_mask_diagnostics_valid, land_mask_diagnostics_valid) \
    X(I, moisture_diagnostics_valid, moisture_diagnostics_valid) \
    X(I, river_diagnostics_valid, river_diagnostics_valid) \
    X(I, metrics_valid, metrics_valid)
#define WORLD_FIELDS(X) \
    X(U64, world_elevation_ms, world.elevation_ms) X(U64, world_mountain_ms, world.mountain_ms) \
    X(U64, world_climate_ms, world.climate_ms) X(U64, world_hydrology_ms, world.hydrology_ms) \
    X(U64, world_classification_ms, world.classification_ms) X(U64, world_commit_ms, world.commit_ms) \
    X(U64, world_total_ms, world.total_ms) X(U64, world_context_bytes, world.context_bytes) \
    X(U64, world_hydrology_bytes, world.hydrology_bytes) X(U64, world_staged_path_bytes, world.staged_path_bytes) \
    X(U64, world_peak_bytes, world.peak_bytes) \
    X(H, physical_hash, world.physical_hash) \
    X(I, world_land_tiles, world.land_tiles) \
    X(I, world_ocean_tiles, world.ocean_tiles) \
    X(I, world_mountain_tiles, world.mountain_tiles) \
    X(I, world_river_tiles, world.river_tiles) \
    X(I, world_river_segments_required, world.river_segments_required) \
    X(I, world_river_segments_copied, world.river_segments_copied)
#define ATTEMPT_FIELDS(X) \
    X(U64, attempt_id, attempt.attempt_id) \
    X(I, attempt_stage, attempt.stage) \
    X(I, attempt_last_failure_stage, attempt.last_failure_stage) \
    X(I, attempt_last_failure_reason, attempt.last_failure_reason) \
    X(I, attempt_active, attempt.active) \
    X(I, attempt_success, attempt.success) \
    X(I, attempt_world_committed, attempt.world_committed) \
    X(I, attempt_snapshot_published, attempt.snapshot_published) \
    X(I, attempt_snapshot_attempts, attempt.snapshot_attempts) \
    X(I, attempt_deferred_snapshot_pending, attempt.deferred_snapshot_pending) \
    X(I, attempt_deferred_snapshot_attempts, attempt.deferred_snapshot_attempts) \
    X(I, attempt_deferred_snapshot_succeeded, attempt.deferred_snapshot_succeeded) \
    X(I, attempt_prewarm_attempted, attempt.prewarm_attempted) \
    X(I, attempt_prewarm_succeeded, attempt.prewarm_succeeded) \
    X(I, attempt_prewarm_attempts, attempt.prewarm_attempts) \
    X(I, attempt_lazy_fallback_required, attempt.lazy_fallback_required) \
    X(I, attempt_target_width, attempt.target_width) \
    X(I, attempt_target_height, attempt.target_height) \
    X(I, attempt_target_land_tiles, attempt.target_land_tiles) \
    X(I, attempt_actual_land_tiles, attempt.actual_land_tiles) \
    X(I, attempt_target_ocean_tiles, attempt.target_ocean_tiles) \
    X(I, attempt_actual_ocean_tiles, attempt.actual_ocean_tiles) \
    X(I, attempt_river_channel_tiles, attempt.river_channel_tiles) \
    X(I, attempt_river_paths_required, attempt.river_paths_required) \
    X(I, attempt_river_paths_copied, attempt.river_paths_copied) \
    X(I, attempt_river_path_capacity, attempt.river_path_capacity) \
    X(U64, attempt_context_allocated_bytes, attempt.context_allocated_bytes) \
    X(I, attempt_context_allocation_failure_field, attempt.context_allocation_failure_field) \
    X(U64, attempt_staged_allocation_bytes, attempt.staged_allocation_bytes) \
    X(U64, attempt_peak_allocation_bytes, attempt.peak_allocation_bytes) \
    X(U64, attempt_elapsed_ms, attempt.elapsed_ms) \
    X(I, attempt_previous_world_generated, attempt.previous_world_generated) \
    X(I, attempt_previous_physical_revision, attempt.previous_physical_revision) \
    X(H, attempt_previous_physical_hash, attempt.previous_physical_hash)
#define LAND_FIELDS(X) \
    X(I, land_target_tiles, land_mask.target_land_tiles) \
    X(I, land_initial_tiles, land_mask.initial_land_tiles) \
    X(I, land_final_tiles, land_mask.final_land_tiles) \
    X(I, land_threshold_elevation, land_mask.threshold_elevation) \
    X(I, land_threshold_candidates, land_mask.threshold_candidates) \
    X(I, land_threshold_selected, land_mask.threshold_selected) \
    X(I, land_frontier_added, land_mask.frontier_added) \
    X(I, land_frontier_removed, land_mask.frontier_removed) \
    X(I, land_frontier_resolutions, land_mask.frontier_resolutions) \
    X(I, land_forced_frontier_seeds, land_mask.forced_frontier_seeds) \
    X(I, land_coastal_lowland_tiles, land_mask.coastal_lowland_tiles) \
    X(I, land_components, land_mask.land_components) \
    X(I, land_water_components, land_mask.water_components) \
    X(I, land_lattice_cells, land_mask.lattice_cells) \
    X(I, land_comb_cells, land_mask.comb_cells) \
    X(I, land_mesh_cells, land_mask.mesh_cells) \
    X(I, land_tendril_cells, land_mask.tendril_cells) \
    X(I, land_threshold_lattice_cells, land_mask.threshold_lattice_cells) \
    X(I, land_threshold_comb_cells, land_mask.threshold_comb_cells) \
    X(I, land_threshold_mesh_cells, land_mask.threshold_mesh_cells) \
    X(I, land_threshold_tendril_cells, land_mask.threshold_tendril_cells) \
    X(I, land_topology_errors, land_mask.topology_errors) \
    X(I, land_target_drift, land_mask.target_drift) \
    X(H, land_mask_hash, land_mask.mask_hash) \
    X(I, land_failure, land_mask.failure)
#define MOISTURE_FIELDS(X) \
    X(I, moisture_diag_tile_count, moisture.tile_count) \
    X(U, moisture_diag_climate_seed, moisture.climate_seed) \
    X(I, moisture_diag_solved_land_tiles, moisture.solved_land_tiles) \
    X(I, moisture_diag_cycle_tiles, moisture.cycle_tiles) \
    X(I, moisture_diag_cycle_count, moisture.cycle_count) \
    X(I, moisture_diag_cycle_iterations, moisture.cycle_iterations) \
    X(I, moisture_diag_max_cycle_iterations, moisture.max_cycle_iterations) \
    X(I, moisture_diag_unconverged_cycles, moisture.unconverged_cycles) \
    X(I, moisture_diag_ocean_reached_land_tiles, moisture.ocean_reached_land_tiles) \
    X(I, moisture_diag_ocean_reached_beyond_20, moisture.ocean_reached_beyond_20) \
    X(I, moisture_diag_max_ocean_chain_length, moisture.max_ocean_chain_length) \
    X(I, moisture_diag_diffusion_passes, moisture.diffusion_passes) \
    X(I, moisture_diag_advection_rounds, moisture.advection_rounds) \
    X(I, moisture_diag_subtile_advection_samples, moisture.subtile_advection_samples) \
    X(I, moisture_diag_subtile_lateral_samples, moisture.subtile_lateral_samples) \
    X(U64, moisture_diag_advection_weight_total, moisture.advection_weight_total) \
    X(U64, moisture_diag_lateral_mix_weight_total, moisture.lateral_mix_weight_total) \
    X(U64, moisture_diag_orographic_precipitation_total, moisture.orographic_precipitation_total) \
    X(U64, moisture_diag_lee_drying_total, moisture.lee_drying_total)
#define RIVER_FIELDS(X) \
    X(U64, river_transient_bytes, river.transient_bytes) \
    X(U64, river_workspace_bytes_required, river.workspace_bytes_required) \
    X(U64, river_workspace_bytes_allocated, river.workspace_bytes_allocated) \
    X(I, river_workspace_allocation_failure_field, river.workspace_allocation_failure_field) \
    X(I, river_workspace_allocation_errors, river.workspace_allocation_errors) \
    X(I, river_land_cells, river.land_cells) \
    X(I, river_topological_cells, river.topological_cells) \
    X(I, river_depression_cells, river.depression_cells) \
    X(I, river_lake_candidate_components, river.lake_candidate_components) \
    X(I, river_lake_qualified_components, river.lake_qualified_components) \
    X(I, river_lake_rejected_components, river.lake_rejected_components) \
    X(I, river_lake_pruned_cells, river.lake_pruned_cells) \
    X(I, river_lake_rejected_cells, river.lake_rejected_cells) \
    X(I, river_lake_reject_area, river.lake_reject_area) \
    X(I, river_lake_reject_depth, river.lake_reject_depth) \
    X(I, river_lake_reject_deep_cells, river.lake_reject_deep_cells) \
    X(I, river_lake_reject_catchment, river.lake_reject_catchment) \
    X(I, river_lake_reject_support, river.lake_reject_support) \
    X(I, river_lake_reject_shape, river.lake_reject_shape) \
    X(I, river_lake_final_invalid_components, river.lake_final_invalid_components) \
    X(I, river_lake_cells, river.lake_cells) \
    X(I, river_closed_basins, river.closed_basins) \
    X(I, river_salt_lakes, river.salt_lakes) \
    X(I, river_channel_cells, river.channel_cells) \
    X(I, river_sources, river.sources) \
    X(I, river_confluences, river.confluences) \
    X(I, river_mouths, river.mouths) \
    X(I, river_deltas, river.deltas) \
    X(I, river_distributaries, river.distributaries) \
    X(I, river_distributary_allocation_errors, river.distributary_allocation_errors) \
    X(I, river_segment_allocation_errors, river.segment_allocation_errors) \
    X(I, river_ordinary_segments, river.ordinary_segments) \
    X(I, river_invalid_receivers, river.invalid_receivers) \
    X(I, river_inland_dead_ends, river.inland_dead_ends) \
    X(I, river_cycle_errors, river.cycle_errors) \
    X(I, river_flow_conservation_errors, river.flow_conservation_errors) \
    X(I, river_width_regressions, river.width_regressions) \
    X(I, river_order_errors, river.order_errors) \
    X(I, river_duplicate_edges, river.duplicate_edges) \
    X(I, river_crossing_repairs, river.crossing_repairs) \
    X(I, river_crossing_errors, river.crossing_errors) \
    X(I, river_receiver_edges, river.receiver_edges) \
    X(I, river_flat_receiver_edges, river.flat_receiver_edges) \
    X(I, river_receiver_lower_index_edges, river.receiver_lower_index_edges) \
    X(I, river_flat_receiver_lower_index_edges, river.flat_receiver_lower_index_edges) \
    X(I, river_max_same_direction_run, river.max_same_direction_run) \
    X(I, river_max_flat_same_direction_run, river.max_flat_same_direction_run) \
    X(I, river_legacy_paths_required, river.legacy_paths_required) \
    X(I, river_legacy_paths_truncated, river.legacy_paths_truncated) \
    X(U, river_channel_threshold, river.channel_threshold) \
    X(U, river_max_flow, river.max_flow) \
    X(I, river_max_order, river.max_order) \
    X(I, river_max_width, river.max_width) \
    X(I, river_bounded_influence_visits, river.bounded_influence_visits)
#define METRIC_FIELDS(X) \
    X(I, tile_count, metrics.tile_count) \
    X(I, land_mask_tiles, metrics.land_mask_tiles) \
    X(I, terrestrial_tiles, metrics.terrestrial_tiles) \
    X(I, ocean_tiles, metrics.ocean_tiles) \
    X(I, lake_tiles, metrics.lake_tiles) \
    X(I, land_mask_nonbinary, metrics.land_mask_nonbinary) \
    X(I, invalid_geography, metrics.invalid_geography) \
    X(I, invalid_climate, metrics.invalid_climate) \
    X(I, invalid_ecology, metrics.invalid_ecology) \
    X(I, invalid_continuous_value, metrics.invalid_continuous_value)
static const char *const GEOGRAPHY_NAMES[GEO_COUNT] = {
    "ocean", "coast", "plain", "hill", "mountain", "plateau", "basin",
    "canyon", "volcano", "lake", "bay", "delta", "wetland", "oasis", "island"
};
static const char *const CLIMATE_NAMES[CLIMATE_COUNT] = {
    "tropical_rainforest", "tropical_monsoon", "tropical_savanna", "desert",
    "semi_arid", "mediterranean", "oceanic", "temperate_monsoon",
    "continental", "subarctic", "tundra", "ice_cap", "alpine",
    "highland_plateau"
};
static const char *const ECOLOGY_NAMES[ECO_COUNT] = {
    "none", "forest", "rainforest", "grassland", "desert", "tundra",
    "swamp", "bamboo", "mangrove"
};
static const char *const DISPLAY_NAMES[CLIMATE_CALIBRATION_GROUP_COUNT] = {
    "icefield", "tundra", "tropical_rainforest", "monsoon", "desert",
    "forest", "temperate_grassland", "other_transition"
};
static int put_separator(FILE *file) {
    return fputc(',', file) != EOF;
}
static int put_header(FILE *file, const char *name, int *first) {
    if (!*first && !put_separator(file)) return 0;
    *first = 0;
    return fputs(name, file) >= 0;
}
static int put_string(FILE *file, const char *value) {
    const char *cursor;
    if (!put_separator(file) || !value || !value[0]) return 0;
    for (cursor = value; *cursor; cursor++) {
        if (*cursor == ',' || *cursor == '\r' || *cursor == '\n') return 0;
    }
    return fputs(value, file) >= 0;
}
static int put_int(FILE *file, int64_t value) {
    return put_separator(file) && fprintf(file, "%lld", (long long)value) >= 0;
}
static int put_uint(FILE *file, uint64_t value) {
    return put_separator(file) &&
           fprintf(file, "%llu", (unsigned long long)value) >= 0;
}
static int put_hash(FILE *file, uint64_t value) {
    return put_separator(file) &&
           fprintf(file, "%016llx", (unsigned long long)value) >= 0;
}
static int put_mean(FILE *file, const ClimateCalibrationDistribution *value) {
    if (!put_separator(file) || !value) return 0;
    if (value->count <= 0) return fputs("-1.000000000", file) >= 0;
    return fprintf(file, "%.9f",
                   (double)value->sum / (double)value->count) >= 0;
}
#define CSV_VALUE_F(v) (fputs((v), file) >= 0)
#define CSV_VALUE_S(v) put_string(file, (v))
#define CSV_VALUE_I(v) put_int(file, (int64_t)(v))
#define CSV_VALUE_U(v) put_uint(file, (uint64_t)(v))
#define CSV_VALUE_U64(v) put_uint(file, (uint64_t)(v))
#define CSV_VALUE_H(v) put_hash(file, (uint64_t)(v))
static int write_named_headers(FILE *file, int *first) {
#define HEADER_FIELD(type, name, expression) \
    if (!put_header(file, #name, first)) return 0;
    BASE_FIELDS(HEADER_FIELD)
    WORLD_FIELDS(HEADER_FIELD)
    ATTEMPT_FIELDS(HEADER_FIELD)
    LAND_FIELDS(HEADER_FIELD)
    MOISTURE_FIELDS(HEADER_FIELD)
    RIVER_FIELDS(HEADER_FIELD)
    METRIC_FIELDS(HEADER_FIELD)
#undef HEADER_FIELD
    return 1;
}
static int write_named_values(FILE *file, const ClimateCalibrationCsvRow *row) {
#define VALUE_FIELD(type, name, expression) \
    if (!CSV_VALUE_##type(row->expression)) return 0;
    BASE_FIELDS(VALUE_FIELD)
    WORLD_FIELDS(VALUE_FIELD)
    ATTEMPT_FIELDS(VALUE_FIELD)
    LAND_FIELDS(VALUE_FIELD)
    MOISTURE_FIELDS(VALUE_FIELD)
    RIVER_FIELDS(VALUE_FIELD)
    METRIC_FIELDS(VALUE_FIELD)
#undef VALUE_FIELD
    return 1;
}
static int token_valid(const char *value, size_t exact_length, int hex_only) {
    size_t length = 0;
    if (!value || !value[0]) return 0;
    while (value[length]) {
        unsigned char c = (unsigned char)value[length];
        if (length >= WORLDGEN_CLIMATE_CALIBRATION_TOKEN_CAPACITY - 1) return 0;
        if (hex_only ? !isxdigit(c) :
            !(isalnum(c) || c == '-' || c == '_' || c == '.')) return 0;
        length++;
    }
    return exact_length == 0 || length == exact_length;
}
int game_worldgen_climate_calibration_identity_valid(
    const ClimateCalibrationIdentity *identity) {
    return identity &&
        strcmp(identity->schema_version,
               WORLDGEN_CLIMATE_CALIBRATION_SCHEMA_VERSION) == 0 &&
        token_valid(identity->run_id, 0, 0) &&
        token_valid(identity->source_head, 40, 1) &&
        token_valid(identity->source_manifest_hash, 64, 1) &&
        token_valid(identity->executable_hash, 64, 1) &&
        token_valid(identity->scripts_manifest_hash, 64, 1) &&
        token_valid(identity->config_manifest_hash, 64, 1);
}
static int world_spec_valid(const ClimateCalibrationWorldSpec *spec) {
    static const int widths[4] = {576, 720, 864, 1152};
    static const int heights[4] = {400, 500, 600, 800};
    static const char *const names[4] = {"Small", "Medium", "Large", "Extreme"};
    if (!spec || spec->seed == 0 ||
        (strcmp(spec->seed_kind, "calibration") != 0 &&
         strcmp(spec->seed_kind, "holdout") != 0) ||
        spec->shard_config_start < 0 || spec->shard_config_count < 1 ||
        spec->shard_config_count > 128 || spec->config_index < 0 ||
        spec->config_index >= 28561 || spec->config_multiplicity < 1 ||
        spec->config_multiplicity > 625 || spec->map_size < 0 ||
        spec->map_size >= 4 || spec->shard_row_index < 0 ||
        spec->shard_config_count > 28561 - spec->shard_config_start ||
        spec->config_index < spec->shard_config_start ||
        spec->config_index >= spec->shard_config_start + spec->shard_config_count ||
        spec->shard_row_index !=
            (spec->config_index - spec->shard_config_start) * 4 + spec->map_size) return 0;
    if (spec->ocean != 50 || spec->continent != 50 || spec->relief != 50 ||
        spec->vegetation != 50 || spec->bias_mountain != 50 ||
        spec->bias_wetland != 50 || spec->random_seed != 0) return 0;
    if (spec->bias_forest < 0 || spec->bias_forest > 100 ||
        spec->bias_desert < 0 || spec->bias_desert > 100 ||
        spec->moisture < 0 || spec->moisture > 100 ||
        spec->drought < 0 || spec->drought > 100) return 0;
    return spec->width == widths[spec->map_size] &&
           spec->height == heights[spec->map_size] &&
           strcmp(spec->map_size_name, names[spec->map_size]) == 0;
}
int game_worldgen_climate_calibration_csv_row_valid(
    const ClimateCalibrationCsvRow *row) {
    if (!row || !game_worldgen_climate_calibration_identity_valid(&row->identity) ||
        !world_spec_valid(&row->spec) ||
        !token_valid(row->failure_stage, 0, 0) ||
        !token_valid(row->failure_reason, 0, 0) ||
        row->attempt.active || row->attempt.world_committed ||
        row->attempt.snapshot_published || row->attempt.snapshot_attempts ||
        row->attempt.prewarm_attempted || row->attempt.prewarm_succeeded ||
        row->attempt.prewarm_attempts) return 0;
    if (!row->success) return strcmp(row->failure_stage, "none") != 0 &&
        strcmp(row->failure_reason, "none") != 0 &&
        ((!row->attempt.success && row->attempt.last_failure_reason !=
          WORLDGEN_FAILURE_NONE) || (row->attempt.success &&
         (!row->world_diagnostics_valid || !row->land_mask_diagnostics_valid ||
          !row->moisture_diagnostics_valid || !row->river_diagnostics_valid ||
          !row->metrics_valid)));
    return strcmp(row->failure_stage, "none") == 0 && strcmp(
        row->failure_reason, "none") == 0 &&
        row->world_diagnostics_valid && row->land_mask_diagnostics_valid &&
        row->moisture_diagnostics_valid && row->river_diagnostics_valid &&
        row->metrics_valid && row->attempt.success &&
        row->attempt.stage == WORLDGEN_ATTEMPT_COMPLETE &&
        row->world.commit_ms == 0 && row->world.physical_hash != 0 &&
        game_worldgen_climate_calibration_metrics_valid(&row->metrics);
}
static int write_array_headers(FILE *file, int *first, const char *prefix,
                               const char *const *names, int count) {
    char name[96];
    int i;
    for (i = 0; i < count; i++) {
        if (snprintf(name, sizeof(name), "%s%s", prefix, names[i]) < 0 ||
            !put_header(file, name, first)) return 0;
    }
    return 1;
}
static int write_distribution_headers(FILE *file, int *first,
                                      const char *prefix) {
    static const char *const suffixes[] = {
        "count", "sum", "min", "max", "mean", "p10", "p50", "p90"
    };
    return write_array_headers(file, first, prefix, suffixes, 8);
}
static int write_distribution(FILE *file,
                              const ClimateCalibrationDistribution *value) {
    return put_int(file, value->count) && put_uint(file, value->sum) &&
        put_int(file, value->minimum) && put_int(file, value->maximum) &&
        put_mean(file, value) && put_int(file, value->p10) &&
        put_int(file, value->p50) && put_int(file, value->p90);
}
int game_worldgen_climate_calibration_csv_write_header(FILE *file) {
    int first = 1;
    int i;
    if (!file || !write_named_headers(file, &first)) return 0;
    for (i = 0; i < 8; i++) {
        char name[64];
        snprintf(name, sizeof(name), "river_receiver_direction_%d", i);
        if (!put_header(file, name, &first)) return 0;
    }
    for (i = 0; i < 8; i++) {
        char name[64];
        snprintf(name, sizeof(name), "river_flat_direction_%d", i);
        if (!put_header(file, name, &first)) return 0;
    }
    if (!write_array_headers(file, &first, "geo_", GEOGRAPHY_NAMES, GEO_COUNT) ||
        !write_array_headers(file, &first, "climate_", CLIMATE_NAMES, CLIMATE_COUNT) ||
        !write_array_headers(file, &first, "ecology_", ECOLOGY_NAMES, ECO_COUNT) ||
        !write_array_headers(file, &first, "display_", DISPLAY_NAMES,
                             CLIMATE_CALIBRATION_GROUP_COUNT) ||
        !write_distribution_headers(file, &first, "temperature_") ||
        !write_distribution_headers(file, &first, "moisture_") ||
        !write_distribution_headers(file, &first, "precipitation_")) return 0;
    return fputc('\n', file) != EOF;
}
static int write_row_values(FILE *file, const ClimateCalibrationCsvRow *row) {
    int i;
    if (!write_named_values(file, row)) return 0;
    for (i = 0; i < 8; i++) {
        if (!put_uint(file, row->river.receiver_direction_histogram[i])) return 0;
    }
    for (i = 0; i < 8; i++) {
        if (!put_uint(file, row->river.flat_direction_histogram[i])) return 0;
    }
    for (i = 0; i < GEO_COUNT; i++) {
        if (!put_uint(file, row->metrics.geography_counts[i])) return 0;
    }
    for (i = 0; i < CLIMATE_COUNT; i++) {
        if (!put_uint(file, row->metrics.climate_counts[i])) return 0;
    }
    for (i = 0; i < ECO_COUNT; i++) {
        if (!put_uint(file, row->metrics.ecology_counts[i])) return 0;
    }
    for (i = 0; i < CLIMATE_CALIBRATION_GROUP_COUNT; i++) {
        if (!put_uint(file, row->metrics.display_group_counts[i])) return 0;
    }
    return write_distribution(file, &row->metrics.temperature) &&
        write_distribution(file, &row->metrics.moisture) &&
        write_distribution(file, &row->metrics.precipitation) &&
        fputc('\n', file) != EOF;
}
int game_worldgen_climate_calibration_csv_column_count(void) {
#define COUNT_FIELD(type, name, expression) + 1
    return 0 BASE_FIELDS(COUNT_FIELD) WORLD_FIELDS(COUNT_FIELD)
        ATTEMPT_FIELDS(COUNT_FIELD) LAND_FIELDS(COUNT_FIELD)
        MOISTURE_FIELDS(COUNT_FIELD) RIVER_FIELDS(COUNT_FIELD)
        METRIC_FIELDS(COUNT_FIELD) + 16 + GEO_COUNT + CLIMATE_COUNT +
        ECO_COUNT + CLIMATE_CALIBRATION_GROUP_COUNT + 24;
#undef COUNT_FIELD
}
int game_worldgen_climate_calibration_csv_open_temp(
    ClimateCalibrationCsvWriter *writer, const char *temporary_path,
    int expected_rows) {
    size_t length;
    if (!writer || !temporary_path || expected_rows <= 0) return 0;
    memset(writer, 0, sizeof(*writer));
    length = strlen(temporary_path);
    if (length < 4 || length >= sizeof(writer->temporary_path) ||
        strcmp(temporary_path + length - 4, ".tmp") != 0 ||
        GetFileAttributesA(temporary_path) != INVALID_FILE_ATTRIBUTES) return 0;
    memcpy(writer->temporary_path, temporary_path, length + 1);
    writer->file = fopen(temporary_path, "wb");
    writer->expected_rows = expected_rows;
    if (!writer->file ||
        !game_worldgen_climate_calibration_csv_write_header(writer->file)) {
        game_worldgen_climate_calibration_csv_abort(writer);
        return 0;
    }
    return 1;
}
int game_worldgen_climate_calibration_csv_write_row(
    ClimateCalibrationCsvWriter *writer,
    const ClimateCalibrationCsvRow *row) {
    if (!writer || !writer->file || writer->failed ||
        writer->rows_written >= writer->expected_rows ||
        !game_worldgen_climate_calibration_csv_row_valid(row) ||
        !write_row_values(writer->file, row)) {
        if (writer) writer->failed = 1;
        return 0;
    }
    writer->rows_written++;
    return 1;
}
int game_worldgen_climate_calibration_csv_close_durable(
    ClimateCalibrationCsvWriter *writer) {
    int ok;
    if (!writer || !writer->file) return 0;
    ok = !writer->failed && writer->rows_written == writer->expected_rows &&
         fflush(writer->file) == 0 && _commit(_fileno(writer->file)) == 0;
    if (fclose(writer->file) != 0) ok = 0;
    writer->file = NULL;
    if (!ok) writer->failed = 1;
    return ok;
}
void game_worldgen_climate_calibration_csv_abort(
    ClimateCalibrationCsvWriter *writer) {
    if (!writer) return;
    if (writer->file) fclose(writer->file);
    writer->file = NULL;
    writer->failed = 1;
}
#undef CSV_VALUE_S
#undef CSV_VALUE_F
#undef CSV_VALUE_I
#undef CSV_VALUE_U
#undef CSV_VALUE_U64
#undef CSV_VALUE_H
