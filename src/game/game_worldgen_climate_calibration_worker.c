#include "game/game_worldgen_climate_calibration_worker.h"

#include "core/worldgen_attempt.h"
#include "game/game_worldgen_climate_calibration_csv.h"
#include "game/game_worldgen_climate_calibration_metrics.h"
#include "world/world_gen.h"
#include "world/world_gen_context.h"
#include "world/world_gen_land_mask.h"
#include "world/world_gen_moisture.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
    WORKER_OPTION_OUTPUT = 1 << 0,
    WORKER_OPTION_RUN_ID = 1 << 1,
    WORKER_OPTION_SOURCE_HEAD = 1 << 2,
    WORKER_OPTION_SOURCE_MANIFEST = 1 << 3,
    WORKER_OPTION_EXECUTABLE = 1 << 4,
    WORKER_OPTION_SCRIPTS = 1 << 5,
    WORKER_OPTION_CONFIG_MANIFEST = 1 << 6,
    WORKER_OPTION_SCHEMA = 1 << 7,
    WORKER_OPTION_SEED = 1 << 8,
    WORKER_OPTION_SEED_KIND = 1 << 9,
    WORKER_OPTION_CONFIG_START = 1 << 10,
    WORKER_OPTION_CONFIG_COUNT = 1 << 11,
    WORKER_OPTION_ALL = (1 << 12) - 1
};

typedef struct {
    char output[WORLDGEN_CLIMATE_CALIBRATION_PATH_CAPACITY];
    ClimateCalibrationIdentity identity;
    uint32_t seed;
    char seed_kind[16];
    int config_start;
    int config_count;
} WorkerOptions;

static const int EFFECTIVE_VALUES[13] = {
    0, 12, 24, 25, 37, 38, 50, 62, 63, 75, 76, 88, 100
};
static const int EFFECTIVE_MULTIPLICITIES[13] = {
    1, 2, 1, 2, 2, 2, 5, 2, 2, 2, 1, 2, 1
};
static const int MAP_WIDTHS[4] = {576, 720, 864, 1152};
static const int MAP_HEIGHTS[4] = {400, 500, 600, 800};
static const char *const MAP_NAMES[4] = {
    "Small", "Medium", "Large", "Extreme"
};

static int copy_text(char *destination, size_t capacity, const char *source) {
    size_t length;
    if (!destination || capacity == 0 || !source) return 0;
    length = strlen(source);
    if (length == 0 || length >= capacity) return 0;
    memcpy(destination, source, length + 1);
    return 1;
}

static int parse_uint32(const char *text, uint32_t *out) {
    unsigned long long value;
    char *end = NULL;
    if (!text || !text[0] || !out || text[0] == '-' || text[0] == '+') return 0;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno || !end || *end || value > UINT32_MAX) return 0;
    *out = (uint32_t)value;
    return 1;
}

static int parse_nonnegative_int(const char *text, int *out) {
    uint32_t value;
    if (!parse_uint32(text, &value) || value > INT_MAX) return 0;
    *out = (int)value;
    return 1;
}

static int seed_kind_valid(uint32_t seed, const char *kind) {
    if (!kind) return 0;
    if (strcmp(kind, "calibration") == 0) {
        return seed == 2026072301u || seed == 2026072302u ||
               seed == 2026072303u || seed == 2026072304u;
    }
    return strcmp(kind, "holdout") == 0 &&
           (seed == 2026072391u || seed == 2026072392u);
}

static int set_option(WorkerOptions *options, int *seen, const char *name,
                      const char *value) {
    int bit = 0;
    int ok = 0;
    if (strcmp(name, "--output") == 0) {
        bit = WORKER_OPTION_OUTPUT;
        ok = copy_text(options->output, sizeof(options->output), value);
    } else if (strcmp(name, "--run-id") == 0) {
        bit = WORKER_OPTION_RUN_ID;
        ok = copy_text(options->identity.run_id,
                       sizeof(options->identity.run_id), value);
    } else if (strcmp(name, "--source-head") == 0) {
        bit = WORKER_OPTION_SOURCE_HEAD;
        ok = copy_text(options->identity.source_head,
                       sizeof(options->identity.source_head), value);
    } else if (strcmp(name, "--source-manifest-hash") == 0) {
        bit = WORKER_OPTION_SOURCE_MANIFEST;
        ok = copy_text(options->identity.source_manifest_hash,
                       sizeof(options->identity.source_manifest_hash), value);
    } else if (strcmp(name, "--executable-hash") == 0) {
        bit = WORKER_OPTION_EXECUTABLE;
        ok = copy_text(options->identity.executable_hash,
                       sizeof(options->identity.executable_hash), value);
    } else if (strcmp(name, "--scripts-manifest-hash") == 0) {
        bit = WORKER_OPTION_SCRIPTS;
        ok = copy_text(options->identity.scripts_manifest_hash,
                       sizeof(options->identity.scripts_manifest_hash), value);
    } else if (strcmp(name, "--config-manifest-hash") == 0) {
        bit = WORKER_OPTION_CONFIG_MANIFEST;
        ok = copy_text(options->identity.config_manifest_hash,
                       sizeof(options->identity.config_manifest_hash), value);
    } else if (strcmp(name, "--schema-version") == 0) {
        bit = WORKER_OPTION_SCHEMA;
        ok = copy_text(options->identity.schema_version,
                       sizeof(options->identity.schema_version), value);
    } else if (strcmp(name, "--seed") == 0) {
        bit = WORKER_OPTION_SEED;
        ok = parse_uint32(value, &options->seed) && options->seed != 0;
    } else if (strcmp(name, "--seed-kind") == 0) {
        bit = WORKER_OPTION_SEED_KIND;
        ok = copy_text(options->seed_kind, sizeof(options->seed_kind), value);
    } else if (strcmp(name, "--config-start") == 0) {
        bit = WORKER_OPTION_CONFIG_START;
        ok = parse_nonnegative_int(value, &options->config_start);
    } else if (strcmp(name, "--config-count") == 0) {
        bit = WORKER_OPTION_CONFIG_COUNT;
        ok = parse_nonnegative_int(value, &options->config_count);
    }
    if (!bit || (*seen & bit) || !ok) return 0;
    *seen |= bit;
    return 1;
}

static int parse_options(int argc, char **argv, WorkerOptions *options) {
    int seen = 0;
    int i;
    if (!options || !argv || argc != 26 || !argv[1] ||
        strcmp(argv[1], "--worldgen-climate-calibration-worker") != 0) return 0;
    memset(options, 0, sizeof(*options));
    for (i = 2; i < argc; i += 2) {
        if (!argv[i] || !argv[i + 1] ||
            !set_option(options, &seen, argv[i], argv[i + 1])) return 0;
    }
    if (seen != WORKER_OPTION_ALL ||
        strcmp(options->identity.schema_version,
               WORLDGEN_CLIMATE_CALIBRATION_SCHEMA_VERSION) != 0 ||
        !game_worldgen_climate_calibration_identity_valid(&options->identity) ||
        !seed_kind_valid(options->seed, options->seed_kind) ||
        options->config_count < 1 || options->config_count > 128 ||
        options->config_start < 0 || options->config_start >= 28561 ||
        options->config_count > 28561 - options->config_start) return 0;
    return 1;
}

static void decode_config(int config_index, ClimateCalibrationWorldSpec *spec) {
    int indices[4];
    int remainder = config_index;
    int i;
    for (i = 3; i >= 0; i--) {
        indices[i] = remainder % 13;
        remainder /= 13;
    }
    spec->bias_forest = EFFECTIVE_VALUES[indices[0]];
    spec->bias_desert = EFFECTIVE_VALUES[indices[1]];
    spec->moisture = EFFECTIVE_VALUES[indices[2]];
    spec->drought = EFFECTIVE_VALUES[indices[3]];
    spec->config_multiplicity = 1;
    for (i = 0; i < 4; i++) {
        spec->config_multiplicity *=
            (uint32_t)EFFECTIVE_MULTIPLICITIES[indices[i]];
    }
}

static WorldGenConfig make_config(const ClimateCalibrationWorldSpec *spec) {
    WorldGenConfig config;
    config.ocean = spec->ocean;
    config.continent = spec->continent;
    config.relief = spec->relief;
    config.moisture = spec->moisture;
    config.drought = spec->drought;
    config.vegetation = spec->vegetation;
    config.bias_forest = spec->bias_forest;
    config.bias_desert = spec->bias_desert;
    config.bias_mountain = spec->bias_mountain;
    config.bias_wetland = spec->bias_wetland;
    config.seed = spec->seed;
    config.random_seed = spec->random_seed;
    return config;
}

static int copy_prepared_diagnostics(ClimateCalibrationCsvRow *row,
                                     const WorldGenContext *context) {
    const WorldGenLandMaskDiagnostics *land;
    const WorldGenMoistureDiagnostics *moisture;
    if (!row || !context) return 0;
    land = world_gen_land_mask_diagnostics(context);
    moisture = world_gen_moisture_last_diagnostics();
    if (land) {
        row->land_mask = *land;
        row->land_mask_diagnostics_valid =
            land->failure == WORLD_GEN_LAND_MASK_OK &&
            land->target_drift == 0 && land->topology_errors == 0 &&
            land->final_land_tiles == land->target_land_tiles;
    }
    if (moisture) {
        row->moisture = *moisture;
        row->moisture_diagnostics_valid =
            moisture->tile_count == context->tile_count &&
            moisture->climate_seed ==
                context->phase_seed[WORLD_GEN_PHASE_CLIMATE] &&
            moisture->unconverged_cycles == 0;
    }
    row->river = context->staged_river_diagnostics;
    row->river_diagnostics_valid =
        context->staged_river_diagnostics_valid &&
        row->river.workspace_allocation_errors == 0 &&
        row->river.distributary_allocation_errors == 0 &&
        row->river.segment_allocation_errors == 0 &&
        row->river.legacy_paths_truncated == 0 &&
        row->river.legacy_paths_required ==
            context->staged_river_paths_required;
    return row->land_mask_diagnostics_valid &&
           row->moisture_diagnostics_valid &&
           row->river_diagnostics_valid;
}

static void set_failure(ClimateCalibrationCsvRow *row, const char *stage,
                        const char *reason) {
    copy_text(row->failure_stage, sizeof(row->failure_stage),
              stage ? stage : "unknown");
    copy_text(row->failure_reason, sizeof(row->failure_reason),
              reason ? reason : "unknown");
}

static int run_world(ClimateCalibrationCsvRow *row) {
    WorldGenConfig config = make_config(&row->spec);
    WorldGenContext *context;
    WorldGenAttemptDiagnostics active_attempt;
    int diagnostics_ok = 0;
    int prepared = 0;
    worldgen_attempt_begin();
    context = world_gen_prepare_for_dimensions(
        &config, row->spec.width, row->spec.height);
    worldgen_attempt_get(&active_attempt);
    if (context ||
        active_attempt.last_failure_stage != WORLDGEN_ATTEMPT_PREPARE_CONTEXT) {
        row->world = *world_gen_last_diagnostics();
        row->world_diagnostics_valid =
            row->world.physical_hash != 0 &&
            row->world.context_bytes > 0 &&
            row->world.peak_bytes >= row->world.context_bytes;
    }
    if (context) {
        prepared = 1;
        diagnostics_ok = copy_prepared_diagnostics(row, context);
        row->metrics_valid =
            game_worldgen_climate_calibration_collect_metrics(
                context, &row->metrics);
        worldgen_attempt_note_elapsed(row->world.total_ms);
        world_gen_release_prepared(context);
    } else if (row->world_diagnostics_valid) {
        worldgen_attempt_note_elapsed(row->world.total_ms);
    }
    worldgen_attempt_finish(prepared);
    worldgen_attempt_get(&row->attempt);
    row->success = prepared && row->world_diagnostics_valid &&
        diagnostics_ok && row->metrics_valid && row->attempt.success &&
        !row->attempt.world_committed && !row->attempt.snapshot_published &&
        !row->attempt.prewarm_attempted;
    if (row->success) {
        set_failure(row, "none", "none");
    } else if (!prepared) {
        set_failure(row,
            worldgen_attempt_stage_name(row->attempt.last_failure_stage),
            worldgen_failure_reason_name(row->attempt.last_failure_reason));
    } else if (!row->metrics_valid) {
        set_failure(row, "metrics", "invalid-prepared-metrics");
    } else {
        set_failure(row, "diagnostics", "invalid-prepared-diagnostics");
    }
    return row->success;
}

static void fill_spec(const WorkerOptions *options, int config_index,
                      int map_size, int row_index,
                      ClimateCalibrationWorldSpec *spec) {
    memset(spec, 0, sizeof(*spec));
    spec->seed = options->seed;
    copy_text(spec->seed_kind, sizeof(spec->seed_kind), options->seed_kind);
    spec->shard_config_start = options->config_start;
    spec->shard_config_count = options->config_count;
    spec->shard_row_index = row_index;
    spec->config_index = config_index;
    spec->ocean = 50;
    spec->continent = 50;
    spec->relief = 50;
    spec->vegetation = 50;
    spec->bias_mountain = 50;
    spec->bias_wetland = 50;
    spec->random_seed = 0;
    decode_config(config_index, spec);
    spec->map_size = map_size;
    copy_text(spec->map_size_name, sizeof(spec->map_size_name),
              MAP_NAMES[map_size]);
    spec->width = MAP_WIDTHS[map_size];
    spec->height = MAP_HEIGHTS[map_size];
}

int run_worldgen_climate_calibration_worker(int argc, char **argv) {
    ClimateCalibrationCsvWriter writer;
    WorkerOptions options;
    int failures = 0;
    int row_index = 0;
    int config_index;
    int map_size;
    if (!parse_options(argc, argv, &options)) return 2;
    if (!game_worldgen_climate_calibration_csv_open_temp(
            &writer, options.output, options.config_count * 4)) return 4;
    for (config_index = options.config_start;
         config_index < options.config_start + options.config_count;
         config_index++) {
        for (map_size = 0; map_size < 4; map_size++) {
            ClimateCalibrationCsvRow row;
            memset(&row, 0, sizeof(row));
            row.identity = options.identity;
            fill_spec(&options, config_index, map_size, row_index, &row.spec);
            if (!run_world(&row)) failures++;
            if (!game_worldgen_climate_calibration_csv_write_row(
                    &writer, &row)) {
                game_worldgen_climate_calibration_csv_abort(&writer);
                return 4;
            }
            row_index++;
        }
    }
    if (!game_worldgen_climate_calibration_csv_close_durable(&writer)) return 4;
    printf("climate calibration shard rows=%d failures=%d output=%s\n",
           row_index, failures, options.output);
    return failures ? 3 : 0;
}

int game_worldgen_climate_calibration_worker_cli_probe(FILE *file) {
    static char hash64[] =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    static char head40[] = "0123456789abcdef0123456789abcdef01234567";
    char *valid[] = {
        "world_sim.exe", "--worldgen-climate-calibration-worker",
        "--output", "probe.csv.tmp", "--run-id", "probe-run",
        "--source-head", head40, "--source-manifest-hash", hash64,
        "--executable-hash", hash64, "--scripts-manifest-hash", hash64,
        "--config-manifest-hash", hash64, "--schema-version",
        WORLDGEN_CLIMATE_CALIBRATION_SCHEMA_VERSION, "--seed", "2026072301",
        "--seed-kind", "calibration", "--config-start", "28433",
        "--config-count", "128"
    };
    WorkerOptions options;
    ClimateCalibrationWorldSpec first = {0};
    ClimateCalibrationWorldSpec last = {0};
    uint64_t multiplicity_sum = 0;
    int i;
    int valid_ok = parse_options(26, valid, &options);
    int overflow_rejected;
    int schema_rejected;
    int seed_kind_rejected;
    int unknown_seed_rejected;
    valid[25] = "129";
    overflow_rejected = !parse_options(26, valid, &options);
    valid[25] = "128";
    valid[17] = "wrong-schema";
    schema_rejected = !parse_options(26, valid, &options);
    valid[17] = WORLDGEN_CLIMATE_CALIBRATION_SCHEMA_VERSION;
    valid[21] = "holdout";
    seed_kind_rejected = !parse_options(26, valid, &options);
    valid[21] = "calibration";
    valid[19] = "1";
    unknown_seed_rejected = !parse_options(26, valid, &options);
    valid[19] = "2026072301";
    decode_config(0, &first);
    decode_config(28560, &last);
    for (i = 0; i < 28561; i++) {
        ClimateCalibrationWorldSpec spec = {0};
        decode_config(i, &spec);
        multiplicity_sum += spec.config_multiplicity;
    }
    i = valid_ok && overflow_rejected && schema_rejected &&
        seed_kind_rejected && unknown_seed_rejected &&
        first.bias_forest == 0 && first.bias_desert == 0 &&
        first.moisture == 0 && first.drought == 0 &&
        last.bias_forest == 100 && last.bias_desert == 100 &&
        last.moisture == 100 && last.drought == 100 &&
        multiplicity_sum == UINT64_C(390625);
    if (file) {
        fprintf(file,
            "case=calibration_worker_cli valid=%d count_rejected=%d "
            "schema_rejected=%d seed_kind_rejected=%d unknown_seed_rejected=%d "
            "configs=28561 multiplicity=%llu ok=%d\n",
            valid_ok, overflow_rejected, schema_rejected, seed_kind_rejected,
            unknown_seed_rejected,
            (unsigned long long)multiplicity_sum, i);
    }
    return i;
}
