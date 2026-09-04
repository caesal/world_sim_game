#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_OPTIONS_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_OPTIONS_H

#include "game/game_worldgen_aridity_response_projection_options.h"

typedef enum {
    GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_FORMULA = 0,
    GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_BASELINE,
    GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_CARRIERS,
    GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_OASIS,
    GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_CONFIRM,
    GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_HOLDOUT,
    GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_PRODUCTION
} GameWorldgenAridityDiminishingAction;

typedef struct {
    GameWorldgenAridityDiminishingAction action;
    const char *action_name;
    GameWorldgenAridityProjectionOptions projection;
    int production_no_override;
} GameWorldgenAridityDiminishingOptions;

int game_worldgen_aridity_diminishing_acknowledgement_matches(
    const char *value);
int game_worldgen_aridity_diminishing_options_parse(
    GameWorldgenAridityDiminishingOptions *options);
int game_worldgen_aridity_diminishing_fixed_candidate_matches(
    const GameWorldgenAridityDiminishingOptions *options);
int game_worldgen_aridity_diminishing_pair_id(
    const GameWorldgenAridityDiminishingOptions *options,
    char *out, size_t capacity);
void game_worldgen_aridity_diminishing_reset_all(void);

#endif
