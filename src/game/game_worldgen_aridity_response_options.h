#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_RESPONSE_OPTIONS_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_RESPONSE_OPTIONS_H

#include <stddef.h>

typedef enum {
    GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_FORMULA = 0,
    GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_BASELINE,
    GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_SCREEN,
    GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_CONFIRM
} GameWorldgenAridityResponseAction;

typedef struct {
    GameWorldgenAridityResponseAction action;
    const char *action_name;
    int use_override;
    int arid_base;
    int desert_bias_span;
    int drought_classification_span;
    int drought_divisor;
    int moisture_compression_span;
    int oasis_drop;
    int transition_margin;
    int emit_artifacts;
} GameWorldgenAridityResponseOptions;

typedef struct {
    int drought_divisor;
    int desert_base;
    int desert_bias_span;
    int semi_arid_width;
    int oasis_transition_margin;
} GameWorldgenAridityResponseDefaults;

int game_worldgen_aridity_response_acknowledgement_matches(const char *value);
int game_worldgen_aridity_response_options_parse(
    GameWorldgenAridityResponseOptions *options);
int game_worldgen_aridity_response_candidate_id(
    const GameWorldgenAridityResponseOptions *options,
    char *out, size_t capacity);
int game_worldgen_aridity_response_pair_id(
    const GameWorldgenAridityResponseOptions *options,
    char *out, size_t capacity);
void game_worldgen_aridity_response_reset_overrides(void);
int game_worldgen_aridity_response_capture_defaults(
    GameWorldgenAridityResponseDefaults *defaults);
int game_worldgen_aridity_response_defaults_restored(
    const GameWorldgenAridityResponseDefaults *defaults);
int game_worldgen_aridity_response_enable_override(
    const GameWorldgenAridityResponseOptions *options);
int game_worldgen_aridity_response_override_matches(
    const GameWorldgenAridityResponseOptions *options);

#endif
