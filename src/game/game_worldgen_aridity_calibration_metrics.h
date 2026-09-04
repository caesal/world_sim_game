#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_CALIBRATION_METRICS_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_CALIBRATION_METRICS_H

#include "core/worldgen_attempt.h"
#include "game/game_worldgen_aridity_calibration_artifacts.h"

#include <stdint.h>
#include <stdio.h>

enum {
    GAME_WORLDGEN_ARIDITY_MAP_COUNT = 4,
    GAME_WORLDGEN_ARIDITY_CASE_COUNT = 6,
    GAME_WORLDGEN_ARIDITY_OASIS_MARGIN_COUNT = 7
};

typedef struct {
    int moisture;
    int drought;
    int bias_desert;
} GameWorldgenAridityCase;

typedef struct {
    int generated;
    uint64_t physical_hash;
    WorldGenAttemptDiagnostics attempt;
    int drought_divisor;
    int desert_base;
    int desert_bias_span;
    int semi_arid_width;
    int oasis_transition_margin;
    int desert_limit;
    int semi_arid_limit;
    int oasis_limit;
    int oasis_transition_limit;
    int land_count;
    int terrestrial_count;
    int lake_count;
    int desert_count;
    int semi_arid_count;
    int combined_arid_count;
    int non_arid_count;
    int oasis_count;
    int wetland_count;
    int river_channel_count;
    int arid_channel_count;
    int oasis_predicate_count;
    int oasis_reachable_count;
    int oasis_suppressed_count;
    int oasis_semantic_errors;
    int projected_visible_oasis_count[
        GAME_WORLDGEN_ARIDITY_OASIS_MARGIN_COUNT];
    int ok;
} GameWorldgenAridityResult;

extern const int game_worldgen_aridity_map_widths[
    GAME_WORLDGEN_ARIDITY_MAP_COUNT];
extern const int game_worldgen_aridity_map_heights[
    GAME_WORLDGEN_ARIDITY_MAP_COUNT];
extern const char *const game_worldgen_aridity_map_names[
    GAME_WORLDGEN_ARIDITY_MAP_COUNT];
extern const GameWorldgenAridityCase game_worldgen_aridity_cases[
    GAME_WORLDGEN_ARIDITY_CASE_COUNT];
extern const int game_worldgen_aridity_oasis_transition_margins[
    GAME_WORLDGEN_ARIDITY_OASIS_MARGIN_COUNT];

uint64_t game_worldgen_aridity_hash_mix(uint64_t hash, uint64_t value);
int game_worldgen_aridity_output_path(char *path, size_t capacity,
                                      const char *name);
int game_worldgen_aridity_output_path_available(const char *path);
int game_worldgen_aridity_run_world(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    GameWorldgenAridityResult *result);
int game_worldgen_aridity_run_world_with_artifacts(
    uint32_t seed, int map_size, const GameWorldgenAridityCase *matrix_case,
    const char *output_directory, const char *artifact_stem,
    GameWorldgenAridityResult *result,
    WorldGenAridityCalibrationArtifactResult *artifact_result);
int game_worldgen_aridity_write_csv_header(FILE *file);
int game_worldgen_aridity_write_csv_row(
    FILE *file, int row_index, uint32_t seed, int map_size,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResult *result);
int game_worldgen_aridity_write_calibration_csv_header(FILE *file);
int game_worldgen_aridity_write_calibration_csv_row(
    FILE *file, int row_index, uint32_t seed, int map_size,
    const GameWorldgenAridityCase *matrix_case,
    const GameWorldgenAridityResult *result);

#endif
