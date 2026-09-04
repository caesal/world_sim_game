#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_CALIBRATION_ARTIFACTS_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_CALIBRATION_ARTIFACTS_H

#include "world/world_gen.h"

#include <stdint.h>

typedef struct {
    uint64_t geography_hash;
    uint64_t climate_hash;
    int width;
    int height;
} WorldGenAridityCalibrationArtifactResult;

int game_worldgen_aridity_calibration_artifacts_write(
    const WorldGenContext *context, const char *output_directory,
    const char *filename_stem,
    WorldGenAridityCalibrationArtifactResult *result);

#endif
