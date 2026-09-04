#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_CALIBRATION_OPTIONS_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_CALIBRATION_OPTIONS_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    GAME_WORLDGEN_ARIDITY_STAGE_QUALIFICATION = 0,
    GAME_WORLDGEN_ARIDITY_STAGE_PILOT,
    GAME_WORLDGEN_ARIDITY_STAGE_SCREENING,
    GAME_WORLDGEN_ARIDITY_STAGE_CALIBRATION,
    GAME_WORLDGEN_ARIDITY_STAGE_CONFIRMATION,
    GAME_WORLDGEN_ARIDITY_STAGE_HOLDOUT,
    GAME_WORLDGEN_ARIDITY_STAGE_FINAL
} GameWorldgenAridityCalibrationStage;

typedef struct {
    GameWorldgenAridityCalibrationStage stage;
    const char *stage_name;
    const char *mode_name;
    const uint32_t *seeds;
    int seed_count;
    int use_override;
    int drought_divisor;
    int desert_base;
    int desert_bias_span;
    int semi_arid_width;
    int oasis_transition_margin;
} GameWorldgenAridityCalibrationOptions;

typedef struct {
    int drought_divisor;
    int desert_base;
    int desert_bias_span;
    int semi_arid_width;
    int oasis_transition_margin;
} GameWorldgenAridityCalibrationDefaults;

int game_worldgen_aridity_calibration_requested(void);
int game_worldgen_aridity_calibration_acknowledgement_matches(
    const char *value);
int game_worldgen_aridity_calibration_options_parse(
    GameWorldgenAridityCalibrationOptions *options);
int game_worldgen_aridity_calibration_candidate_id(
    const GameWorldgenAridityCalibrationOptions *options,
    char *out, size_t capacity);
void game_worldgen_aridity_calibration_reset_overrides(void);
int game_worldgen_aridity_calibration_capture_defaults(
    GameWorldgenAridityCalibrationDefaults *defaults);
int game_worldgen_aridity_calibration_defaults_restored(
    const GameWorldgenAridityCalibrationDefaults *defaults);
int game_worldgen_aridity_calibration_enable_override(
    const GameWorldgenAridityCalibrationOptions *options);
int game_worldgen_aridity_calibration_override_matches(
    const GameWorldgenAridityCalibrationOptions *options);

#endif
