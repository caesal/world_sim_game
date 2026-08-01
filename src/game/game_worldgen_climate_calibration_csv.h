#ifndef WORLD_SIM_GAME_WORLDGEN_CLIMATE_CALIBRATION_CSV_H
#define WORLD_SIM_GAME_WORLDGEN_CLIMATE_CALIBRATION_CSV_H

#include <stdio.h>
#include <stdint.h>

#include "core/worldgen_attempt.h"
#include "game/game_worldgen_climate_calibration_metrics.h"
#include "world/river_types.h"
#include "world/world_gen.h"
#include "world/world_gen_land_mask.h"
#include "world/world_gen_moisture.h"

#define WORLDGEN_CLIMATE_CALIBRATION_SCHEMA_VERSION \
    "worldgen-climate-calibration-v1"
#define WORLDGEN_CLIMATE_CALIBRATION_TOKEN_CAPACITY 96
#define WORLDGEN_CLIMATE_CALIBRATION_PATH_CAPACITY 1024

typedef struct {
    char schema_version[WORLDGEN_CLIMATE_CALIBRATION_TOKEN_CAPACITY];
    char run_id[WORLDGEN_CLIMATE_CALIBRATION_TOKEN_CAPACITY];
    char source_head[WORLDGEN_CLIMATE_CALIBRATION_TOKEN_CAPACITY];
    char source_manifest_hash[WORLDGEN_CLIMATE_CALIBRATION_TOKEN_CAPACITY];
    char executable_hash[WORLDGEN_CLIMATE_CALIBRATION_TOKEN_CAPACITY];
    char scripts_manifest_hash[WORLDGEN_CLIMATE_CALIBRATION_TOKEN_CAPACITY];
    char config_manifest_hash[WORLDGEN_CLIMATE_CALIBRATION_TOKEN_CAPACITY];
} ClimateCalibrationIdentity;

typedef struct {
    uint32_t seed;
    char seed_kind[16];
    int shard_config_start;
    int shard_config_count;
    int shard_row_index;
    int config_index;
    int ocean;
    int continent;
    int relief;
    int vegetation;
    int bias_forest;
    int bias_desert;
    int bias_mountain;
    int bias_wetland;
    int moisture;
    int drought;
    int random_seed;
    uint32_t config_multiplicity;
    int map_size;
    char map_size_name[16];
    int width;
    int height;
} ClimateCalibrationWorldSpec;

typedef struct {
    ClimateCalibrationIdentity identity;
    ClimateCalibrationWorldSpec spec;
    int success;
    char failure_stage[48];
    char failure_reason[80];
    int world_diagnostics_valid;
    int land_mask_diagnostics_valid;
    int moisture_diagnostics_valid;
    int river_diagnostics_valid;
    int metrics_valid;
    WorldGenDiagnostics world;
    WorldGenAttemptDiagnostics attempt;
    WorldGenLandMaskDiagnostics land_mask;
    WorldGenMoistureDiagnostics moisture;
    RiverGenerationDiagnostics river;
    ClimateCalibrationMetrics metrics;
} ClimateCalibrationCsvRow;

typedef struct {
    FILE *file;
    int expected_rows;
    int rows_written;
    int failed;
    char temporary_path[WORLDGEN_CLIMATE_CALIBRATION_PATH_CAPACITY];
} ClimateCalibrationCsvWriter;

int game_worldgen_climate_calibration_identity_valid(
    const ClimateCalibrationIdentity *identity);
int game_worldgen_climate_calibration_csv_row_valid(
    const ClimateCalibrationCsvRow *row);
int game_worldgen_climate_calibration_csv_column_count(void);
int game_worldgen_climate_calibration_csv_write_header(FILE *file);
int game_worldgen_climate_calibration_csv_open_temp(
    ClimateCalibrationCsvWriter *writer, const char *temporary_path,
    int expected_rows);
int game_worldgen_climate_calibration_csv_write_row(
    ClimateCalibrationCsvWriter *writer,
    const ClimateCalibrationCsvRow *row);
int game_worldgen_climate_calibration_csv_close_durable(
    ClimateCalibrationCsvWriter *writer);
void game_worldgen_climate_calibration_csv_abort(
    ClimateCalibrationCsvWriter *writer);

#endif
