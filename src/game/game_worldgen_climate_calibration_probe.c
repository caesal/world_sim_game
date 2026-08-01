#include "game/game_worldgen_climate_calibration_probe.h"

#include "core/worldgen_attempt.h"
#include "game/game_worldgen_climate_calibration_csv.h"
#include "game/game_worldgen_climate_calibration_metrics.h"
#include "game/game_worldgen_climate_calibration_worker.h"
#include "world/world_gen.h"
#include "world/world_gen_context.h"
#include "world/world_physical_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define PROBE_SUMMARY_ENV \
    "WORLD_SIM_WORLDGEN_CLIMATE_CALIBRATION_PROBE_SUMMARY"
#define PROBE_DIRECTORY_ENV \
    "WORLD_SIM_WORLDGEN_CLIMATE_CALIBRATION_PROBE_DIR"

static void set_tile(WorldGenContext *context, int index, int land,
                     Geography geography, Climate climate, Ecology ecology,
                     int temperature, int moisture, int precipitation) {
    context->land_mask[index] = (uint8_t)land;
    context->geography[index] = (uint8_t)geography;
    context->climate[index] = (uint8_t)climate;
    context->ecology[index] = (uint8_t)ecology;
    context->temperature[index] = (int16_t)temperature;
    context->moisture[index] = (int16_t)moisture;
    context->precipitation[index] = (int16_t)precipitation;
}

static int synthetic_metrics_case(FILE *file,
                                  ClimateCalibrationMetrics *valid_metrics) {
    WorldGenContext context;
    uint8_t land_mask[10] = {0};
    uint8_t geography[10] = {0};
    uint8_t climate[10] = {0};
    uint8_t ecology[10] = {0};
    int16_t temperature[10] = {0};
    int16_t moisture[10] = {0};
    int16_t precipitation[10] = {0};
    ClimateCalibrationMetrics metrics;
    int groups_ok = 1;
    int invalid_rejected;
    int i;
    memset(&context, 0, sizeof(context));
    context.width = 10;
    context.height = 1;
    context.tile_count = 10;
    context.land_mask = land_mask;
    context.geography = geography;
    context.climate = climate;
    context.ecology = ecology;
    context.temperature = temperature;
    context.moisture = moisture;
    context.precipitation = precipitation;
    set_tile(&context, 0, 0, GEO_OCEAN, CLIMATE_OCEANIC, ECO_NONE, 50, 50, 50);
    set_tile(&context, 1, 1, GEO_LAKE, CLIMATE_OCEANIC, ECO_NONE, 50, 50, 50);
    set_tile(&context, 2, 1, GEO_PLAIN, CLIMATE_ICE_CAP, ECO_FOREST, 10, 80, 50);
    set_tile(&context, 3, 1, GEO_PLAIN, CLIMATE_SUBARCTIC, ECO_RAINFOREST, 20, 70, 50);
    set_tile(&context, 4, 1, GEO_PLAIN, CLIMATE_TROPICAL_RAINFOREST,
             ECO_DESERT, 30, 60, 50);
    set_tile(&context, 5, 1, GEO_PLAIN, CLIMATE_TEMPERATE_MONSOON,
             ECO_DESERT, 40, 50, 50);
    set_tile(&context, 6, 1, GEO_PLAIN, CLIMATE_DESERT, ECO_FOREST, 50, 40, 50);
    set_tile(&context, 7, 1, GEO_PLAIN, CLIMATE_TROPICAL_SAVANNA,
             ECO_MANGROVE, 60, 30, 50);
    set_tile(&context, 8, 1, GEO_PLAIN, CLIMATE_CONTINENTAL,
             ECO_GRASSLAND, 70, 20, 50);
    set_tile(&context, 9, 1, GEO_PLAIN, CLIMATE_TROPICAL_SAVANNA,
             ECO_GRASSLAND, 80, 10, 50);
    if (!game_worldgen_climate_calibration_collect_metrics(&context, &metrics)) {
        if (file) fprintf(file, "case=calibration_metrics collected=0 ok=0\n");
        return 0;
    }
    for (i = 0; i < CLIMATE_CALIBRATION_GROUP_COUNT; i++) {
        groups_ok &= metrics.display_group_counts[i] == 1u;
    }
    if (valid_metrics) *valid_metrics = metrics;
    temperature[2] = 101;
    invalid_rejected =
        !game_worldgen_climate_calibration_collect_metrics(&context, &metrics);
    i = groups_ok && invalid_rejected &&
        valid_metrics && valid_metrics->tile_count == 10 &&
        valid_metrics->land_mask_tiles == 9 &&
        valid_metrics->terrestrial_tiles == 8 &&
        valid_metrics->ocean_tiles == 1 && valid_metrics->lake_tiles == 1 &&
        valid_metrics->temperature.minimum == 10 &&
        valid_metrics->temperature.maximum == 80 &&
        valid_metrics->temperature.p10 == 10 &&
        valid_metrics->temperature.p50 == 40 &&
        valid_metrics->temperature.p90 == 80 &&
        valid_metrics->temperature.sum == 360u;
    if (file) {
        fprintf(file,
            "case=calibration_metrics tiles=%d land=%d terrestrial=%d "
            "ocean=%d lake=%d groups=%d percentiles=%d/%d/%d "
            "invalid_rejected=%d ok=%d\n",
            valid_metrics ? valid_metrics->tile_count : -1,
            valid_metrics ? valid_metrics->land_mask_tiles : -1,
            valid_metrics ? valid_metrics->terrestrial_tiles : -1,
            valid_metrics ? valid_metrics->ocean_tiles : -1,
            valid_metrics ? valid_metrics->lake_tiles : -1, groups_ok,
            valid_metrics ? valid_metrics->temperature.p10 : -1,
            valid_metrics ? valid_metrics->temperature.p50 : -1,
            valid_metrics ? valid_metrics->temperature.p90 : -1,
            invalid_rejected, i);
    }
    return i;
}

static void fill_probe_identity(ClimateCalibrationIdentity *identity) {
    static const char hash[] =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    memset(identity, 0, sizeof(*identity));
    snprintf(identity->schema_version, sizeof(identity->schema_version), "%s",
             WORLDGEN_CLIMATE_CALIBRATION_SCHEMA_VERSION);
    snprintf(identity->run_id, sizeof(identity->run_id), "focused-probe");
    snprintf(identity->source_head, sizeof(identity->source_head),
             "0123456789abcdef0123456789abcdef01234567");
    snprintf(identity->source_manifest_hash,
             sizeof(identity->source_manifest_hash), "%s", hash);
    snprintf(identity->executable_hash,
             sizeof(identity->executable_hash), "%s", hash);
    snprintf(identity->scripts_manifest_hash,
             sizeof(identity->scripts_manifest_hash), "%s", hash);
    snprintf(identity->config_manifest_hash,
             sizeof(identity->config_manifest_hash), "%s", hash);
}

static int line_columns(const char *line) {
    int columns = line && line[0] ? 1 : 0;
    if (!line) return 0;
    while (*line) if (*line++ == ',') columns++;
    return columns;
}

static int csv_case(FILE *file, const ClimateCalibrationMetrics *metrics) {
    const char *directory = getenv(PROBE_DIRECTORY_ENV);
    ClimateCalibrationCsvWriter writer;
    ClimateCalibrationCsvRow row;
    char path[WORLDGEN_CLIMATE_CALIBRATION_PATH_CAPACITY];
    char *header = NULL;
    char *data = NULL;
    FILE *input = NULL;
    int columns = game_worldgen_climate_calibration_csv_column_count();
    int failure_contract;
    int ok = 0;
    if (!directory || !directory[0]) {
        CreateDirectoryA("logs", NULL);
        directory = "logs";
    }
    snprintf(path, sizeof(path), "%s/calibration_csv_probe_%lu_%llu.csv.tmp",
             directory, (unsigned long)GetCurrentProcessId(),
             (unsigned long long)GetTickCount64());
    memset(&row, 0, sizeof(row));
    fill_probe_identity(&row.identity);
    row.spec.seed = 2026072301u;
    snprintf(row.spec.seed_kind, sizeof(row.spec.seed_kind), "calibration");
    row.spec.shard_config_count = 1;
    row.spec.ocean = 50;
    row.spec.continent = 50;
    row.spec.relief = 50;
    row.spec.vegetation = 50;
    row.spec.bias_mountain = 50;
    row.spec.bias_wetland = 50;
    row.spec.config_multiplicity = 1;
    snprintf(row.spec.map_size_name, sizeof(row.spec.map_size_name), "Small");
    row.spec.width = 576;
    row.spec.height = 400;
    row.success = 1;
    snprintf(row.failure_stage, sizeof(row.failure_stage), "none");
    snprintf(row.failure_reason, sizeof(row.failure_reason), "none");
    row.world_diagnostics_valid = 1;
    row.land_mask_diagnostics_valid = 1;
    row.moisture_diagnostics_valid = 1;
    row.river_diagnostics_valid = 1;
    row.metrics_valid = 1;
    row.world.context_bytes = 1;
    row.world.peak_bytes = 1;
    row.world.physical_hash = UINT64_C(1);
    row.attempt.stage = WORLDGEN_ATTEMPT_COMPLETE;
    row.attempt.success = 1;
    row.metrics = *metrics;
    row.success = 0;
    row.metrics_valid = 0;
    failure_contract =
        !game_worldgen_climate_calibration_csv_row_valid(&row);
    snprintf(row.failure_stage, sizeof(row.failure_stage), "metrics");
    snprintf(row.failure_reason, sizeof(row.failure_reason), "invalid-metrics");
    failure_contract &=
        game_worldgen_climate_calibration_csv_row_valid(&row);
    row.success = 1;
    row.metrics_valid = 1;
    snprintf(row.failure_stage, sizeof(row.failure_stage), "none");
    snprintf(row.failure_reason, sizeof(row.failure_reason), "none");
    if (!game_worldgen_climate_calibration_csv_open_temp(&writer, path, 1) ||
        !game_worldgen_climate_calibration_csv_write_row(&writer, &row) ||
        !game_worldgen_climate_calibration_csv_close_durable(&writer)) goto done;
    header = (char *)malloc(65536);
    data = (char *)malloc(65536);
    input = fopen(path, "rb");
    if (!header || !data || !input ||
        !fgets(header, 65536, input) || !fgets(data, 65536, input)) goto done;
    ok = failure_contract && line_columns(header) == columns &&
         line_columns(data) == columns &&
         fgetc(input) == EOF &&
         GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
done:
    if (input) fclose(input);
    if (file && header && header[0]) {
        fprintf(file, "schema_version=%s columns=%d\nschema_header=%s",
                WORLDGEN_CLIMATE_CALIBRATION_SCHEMA_VERSION, columns, header);
    }
    free(header);
    free(data);
    if (file) {
        fprintf(file,
            "case=calibration_csv columns=%d failure_contract=%d "
            "durable_tmp=%d path=%s ok=%d\n",
            columns, failure_contract,
            GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES,
            path, ok);
    }
    return ok;
}

static int lifecycle_once(uint64_t *physical_hash,
                          ClimateCalibrationMetrics *metrics) {
    WorldGenConfig config;
    WorldGenContext *context;
    WorldGenAttemptDiagnostics attempt;
    int revision_before = world_physical_state_revision();
    int prepared = 0;
    int metrics_ok = 0;
    config.ocean = 50;
    config.continent = 50;
    config.relief = 50;
    config.moisture = 62;
    config.drought = 37;
    config.vegetation = 50;
    config.bias_forest = 63;
    config.bias_desert = 38;
    config.bias_mountain = 50;
    config.bias_wetland = 50;
    config.seed = 2026072301u;
    config.random_seed = 0;
    worldgen_attempt_begin();
    context = world_gen_prepare_for_dimensions(&config, 96, 64);
    if (context) {
        prepared = 1;
        *physical_hash = world_gen_last_diagnostics()->physical_hash;
        metrics_ok = game_worldgen_climate_calibration_collect_metrics(
            context, metrics);
        worldgen_attempt_note_elapsed(world_gen_last_diagnostics()->total_ms);
        world_gen_release_prepared(context);
    }
    worldgen_attempt_finish(prepared && metrics_ok);
    worldgen_attempt_get(&attempt);
    return prepared && metrics_ok && *physical_hash != 0 && !attempt.active &&
        attempt.success && !attempt.world_committed &&
        !attempt.snapshot_published && !attempt.prewarm_attempted &&
        world_physical_state_revision() == revision_before;
}

static int lifecycle_case(FILE *file) {
    ClimateCalibrationMetrics first_metrics;
    ClimateCalibrationMetrics repeat_metrics;
    uint64_t first_hash = 0;
    uint64_t repeat_hash = 0;
    int first = lifecycle_once(&first_hash, &first_metrics);
    int repeat = lifecycle_once(&repeat_hash, &repeat_metrics);
    int ok = first && repeat && first_hash == repeat_hash &&
        memcmp(&first_metrics, &repeat_metrics, sizeof(first_metrics)) == 0;
    if (file) {
        fprintf(file,
            "case=calibration_lifecycle first=%d repeat=%d "
            "hash=%016llx/%016llx metrics_equal=%d ok=%d\n",
            first, repeat, (unsigned long long)first_hash,
            (unsigned long long)repeat_hash,
            memcmp(&first_metrics, &repeat_metrics,
                   sizeof(first_metrics)) == 0, ok);
    }
    return ok;
}

int run_worldgen_climate_calibration_probe(void) {
    const char *summary_path = getenv(PROBE_SUMMARY_ENV);
    ClimateCalibrationMetrics metrics;
    FILE *file;
    int ok;
    if (!summary_path || !summary_path[0]) {
        CreateDirectoryA("logs", NULL);
        summary_path = "logs/worldgen_climate_calibration_probe.txt";
    }
    file = fopen(summary_path, "wb");
    if (!file) return 2;
    ok = game_worldgen_climate_calibration_worker_cli_probe(file);
    ok &= synthetic_metrics_case(file, &metrics);
    ok &= csv_case(file, &metrics);
    ok &= lifecycle_case(file);
    fprintf(file, "overall_ok=%d\n", ok);
    if (fflush(file) != 0 || fclose(file) != 0) return 2;
    printf("worldgen climate calibration probe summary: %s\n", summary_path);
    return ok ? 0 : 1;
}
