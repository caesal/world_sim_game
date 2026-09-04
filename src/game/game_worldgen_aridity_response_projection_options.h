#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_RESPONSE_PROJECTION_OPTIONS_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_RESPONSE_PROJECTION_OPTIONS_H

#include "game/game_worldgen_aridity_response_options.h"

#include <stddef.h>

typedef enum {
    GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_FORMULA = 0,
    GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_BASELINE,
    GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CARRIERS,
    GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_OASIS,
    GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CONFIRM
} GameWorldgenAridityProjectionAction;

typedef struct {
    GameWorldgenAridityProjectionAction action;
    const char *action_name;
    GameWorldgenAridityResponseOptions response;
    int has_candidate;
    int has_oasis_pair;
    int emit_artifacts;
} GameWorldgenAridityProjectionOptions;

int game_worldgen_aridity_projection_acknowledgement_matches(
    const char *value);
int game_worldgen_aridity_projection_options_parse(
    GameWorldgenAridityProjectionOptions *options);
int game_worldgen_aridity_projection_candidate_id(
    const GameWorldgenAridityProjectionOptions *options,
    char *out, size_t capacity);
int game_worldgen_aridity_projection_pair_id(
    const GameWorldgenAridityProjectionOptions *options,
    char *out, size_t capacity);
void game_worldgen_aridity_projection_reset_all(void);
int game_worldgen_aridity_projection_capture_defaults(
    GameWorldgenAridityResponseDefaults *defaults);
int game_worldgen_aridity_projection_defaults_restored(
    const GameWorldgenAridityResponseDefaults *defaults);
int game_worldgen_aridity_projection_enable_carrier(int drought_divisor);
int game_worldgen_aridity_projection_carrier_matches(int drought_divisor);
int game_worldgen_aridity_projection_enable_actual(
    const GameWorldgenAridityProjectionOptions *options);
int game_worldgen_aridity_projection_actual_matches(
    const GameWorldgenAridityProjectionOptions *options);

#endif
