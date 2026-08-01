#ifndef WORLD_SIM_GAME_WORLDGEN_CLIMATE_CALIBRATION_METRICS_H
#define WORLD_SIM_GAME_WORLDGEN_CLIMATE_CALIBRATION_METRICS_H

#include <stdint.h>

#include "core/world_types.h"

typedef struct WorldGenContext WorldGenContext;

typedef enum {
    CLIMATE_CALIBRATION_GROUP_ICEFIELD = 0,
    CLIMATE_CALIBRATION_GROUP_TUNDRA,
    CLIMATE_CALIBRATION_GROUP_TROPICAL_RAINFOREST,
    CLIMATE_CALIBRATION_GROUP_MONSOON,
    CLIMATE_CALIBRATION_GROUP_DESERT,
    CLIMATE_CALIBRATION_GROUP_FOREST,
    CLIMATE_CALIBRATION_GROUP_TEMPERATE_GRASSLAND,
    CLIMATE_CALIBRATION_GROUP_OTHER_TRANSITION,
    CLIMATE_CALIBRATION_GROUP_COUNT
} ClimateCalibrationDisplayGroup;

typedef struct {
    int count;
    int minimum;
    int maximum;
    int p10;
    int p50;
    int p90;
    uint64_t sum;
} ClimateCalibrationDistribution;

typedef struct {
    int valid;
    int tile_count;
    int land_mask_tiles;
    int terrestrial_tiles;
    int ocean_tiles;
    int lake_tiles;
    int land_mask_nonbinary;
    uint64_t geography_counts[GEO_COUNT];
    uint64_t climate_counts[CLIMATE_COUNT];
    uint64_t ecology_counts[ECO_COUNT];
    uint64_t display_group_counts[CLIMATE_CALIBRATION_GROUP_COUNT];
    int invalid_geography;
    int invalid_climate;
    int invalid_ecology;
    int invalid_continuous_value;
    ClimateCalibrationDistribution temperature;
    ClimateCalibrationDistribution moisture;
    ClimateCalibrationDistribution precipitation;
} ClimateCalibrationMetrics;

int game_worldgen_climate_calibration_collect_metrics(
    const WorldGenContext *context, ClimateCalibrationMetrics *out);
int game_worldgen_climate_calibration_metrics_valid(
    const ClimateCalibrationMetrics *metrics);

#endif
