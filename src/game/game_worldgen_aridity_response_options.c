#include "game/game_worldgen_aridity_response_options.h"
#include "game/game_worldgen_aridity_response_metrics.h"

#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_moisture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESPONSE_ACK_ENV "WORLD_SIM_ARIDITY_CALIBRATION_ACK"
#define RESPONSE_ACTION_ENV "WORLD_SIM_ARIDITY_RESPONSE_ACTION"
#define RESPONSE_DROUGHT_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_DROUGHT_DIVISOR"
#define RESPONSE_COMPRESSION_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_MOISTURE_COMPRESSION_SPAN"
#define RESPONSE_OASIS_DROP_ENV "WORLD_SIM_ARIDITY_RESPONSE_OASIS_DROP"
#define RESPONSE_TRANSITION_ENV "WORLD_SIM_ARIDITY_RESPONSE_TRANSITION_MARGIN"
#define RESPONSE_ARTIFACT_ENV "WORLD_SIM_ARIDITY_RESPONSE_EMIT_ARTIFACTS"
#define RESPONSE_ACK "VER037A_ARIDITY_RESPONSE_PILOT"
#define RESPONSE_ARID_BASE 30
#define RESPONSE_DESERT_BIAS_SPAN 2

static int parse_int(const char *text, int *value) {
    char *end = NULL;
    long parsed;
    if (!text || !text[0] || !value) return 0;
    parsed = strtol(text, &end, 10);
    if (!end || *end != '\0' || parsed < 0 || parsed > 100) return 0;
    *value = (int)parsed;
    return 1;
}

static int value_in(const int value, const int *values, int count) {
    int index;
    for (index = 0; index < count; index++) {
        if (value == values[index]) return 1;
    }
    return 0;
}

static int candidate_in_grid(int drought_divisor, int compression_span) {
    static const int drought_values[] = {20, 24, 28};
    static const int compression_values[] = {6, 8, 10};
    return value_in(drought_divisor, drought_values, 3) &&
        value_in(compression_span, compression_values, 3);
}

int game_worldgen_aridity_response_acknowledgement_matches(
    const char *value) {
    return value && strcmp(value, RESPONSE_ACK) == 0;
}

static int assign_action(
    const char *name, GameWorldgenAridityResponseOptions *options) {
    if (strcmp(name, "formula") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_FORMULA;
    } else if (strcmp(name, "baseline") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_BASELINE;
    } else if (strcmp(name, "screen") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_SCREEN;
    } else if (strcmp(name, "confirm") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_CONFIRM;
    } else {
        return 0;
    }
    options->action_name = name;
    return 1;
}

int game_worldgen_aridity_response_options_parse(
    GameWorldgenAridityResponseOptions *options) {
    const char *ack = getenv(RESPONSE_ACK_ENV);
    const char *action = getenv(RESPONSE_ACTION_ENV);
    const char *drought = getenv(RESPONSE_DROUGHT_ENV);
    const char *compression = getenv(RESPONSE_COMPRESSION_ENV);
    const char *drop = getenv(RESPONSE_OASIS_DROP_ENV);
    const char *transition = getenv(RESPONSE_TRANSITION_ENV);
    const char *artifacts = getenv(RESPONSE_ARTIFACT_ENV);
    if (!options || !action || !action[0]) return 0;
    memset(options, 0, sizeof(*options));
    if (!assign_action(action, options)) return 0;
    if (options->action == GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_BASELINE) {
        return !ack && !drought && !compression && !drop && !transition &&
            !artifacts;
    }
    if (!game_worldgen_aridity_response_acknowledgement_matches(ack)) return 0;
    if (options->action == GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_FORMULA) {
        return !drought && !compression && !drop && !transition && !artifacts;
    }
    if (!parse_int(drought, &options->drought_divisor) ||
        !parse_int(compression, &options->moisture_compression_span) ||
        !parse_int(drop, &options->oasis_drop) ||
        !parse_int(transition, &options->transition_margin) ||
        !candidate_in_grid(options->drought_divisor,
                           options->moisture_compression_span) ||
        !game_worldgen_aridity_response_oasis_pair_valid(
            options->oasis_drop, options->transition_margin)) {
        return 0;
    }
    options->use_override = 1;
    options->arid_base = RESPONSE_ARID_BASE;
    options->desert_bias_span = RESPONSE_DESERT_BIAS_SPAN;
    if (options->action == GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_SCREEN) {
        return options->oasis_drop == 8 && options->transition_margin == 0 &&
            !artifacts;
    }
    options->emit_artifacts = artifacts && strcmp(artifacts, "1") == 0;
    return options->action == GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_CONFIRM &&
        options->emit_artifacts;
}

int game_worldgen_aridity_response_candidate_id(
    const GameWorldgenAridityResponseOptions *options,
    char *out, size_t capacity) {
    int written;
    if (!options || !out || capacity == 0 || !options->use_override) return 0;
    written = snprintf(out, capacity, "d%d_c%d", options->drought_divisor,
                       options->moisture_compression_span);
    return written > 0 && (size_t)written < capacity;
}

int game_worldgen_aridity_response_pair_id(
    const GameWorldgenAridityResponseOptions *options,
    char *out, size_t capacity) {
    int written;
    if (!options || !out || capacity == 0 || !options->use_override) return 0;
    written = snprintf(out, capacity, "o%02d_t%02d", options->oasis_drop,
                       options->transition_margin);
    return written > 0 && (size_t)written < capacity;
}

void game_worldgen_aridity_response_reset_overrides(void) {
    world_gen_aridity_response_validation_reset();
    world_gen_classify_validation_reset_aridity();
    world_gen_moisture_validation_reset_drought_divisor();
}

int game_worldgen_aridity_response_capture_defaults(
    GameWorldgenAridityResponseDefaults *defaults) {
    if (!defaults) return 0;
    game_worldgen_aridity_response_reset_overrides();
    defaults->drought_divisor = world_gen_moisture_drought_divisor();
    defaults->desert_base = world_gen_desert_base();
    defaults->desert_bias_span = world_gen_desert_bias_span();
    defaults->semi_arid_width = world_gen_semi_arid_width();
    defaults->oasis_transition_margin = world_gen_oasis_transition_margin();
    return game_worldgen_aridity_response_defaults_restored(defaults);
}

int game_worldgen_aridity_response_defaults_restored(
    const GameWorldgenAridityResponseDefaults *defaults) {
    return defaults &&
        !world_gen_moisture_validation_drought_divisor_active() &&
        !world_gen_classify_validation_aridity_active() &&
        !world_gen_aridity_response_validation_active() &&
        world_gen_moisture_drought_divisor() == defaults->drought_divisor &&
        world_gen_desert_base() == defaults->desert_base &&
        world_gen_desert_bias_span() == defaults->desert_bias_span &&
        world_gen_semi_arid_width() == defaults->semi_arid_width &&
        world_gen_oasis_transition_margin() ==
            defaults->oasis_transition_margin;
}

int game_worldgen_aridity_response_enable_override(
    const GameWorldgenAridityResponseOptions *options) {
    game_worldgen_aridity_response_reset_overrides();
    if (!options || !options->use_override ||
        !world_gen_moisture_validation_set_drought_divisor(
            options->drought_divisor) ||
        !world_gen_aridity_response_validation_enable_diminishing(
            options->arid_base, options->desert_bias_span,
            options->drought_classification_span,
            options->moisture_compression_span, options->oasis_drop,
            options->transition_margin) ||
        !game_worldgen_aridity_response_override_matches(options)) {
        game_worldgen_aridity_response_reset_overrides();
        return 0;
    }
    return 1;
}

int game_worldgen_aridity_response_override_matches(
    const GameWorldgenAridityResponseOptions *options) {
    return options && options->use_override &&
        world_gen_moisture_validation_drought_divisor_active() &&
        !world_gen_classify_validation_aridity_active() &&
        world_gen_aridity_response_validation_active() &&
        world_gen_moisture_drought_divisor() == options->drought_divisor &&
        world_gen_aridity_response_validation_matches_diminishing(
            options->arid_base, options->desert_bias_span,
            options->drought_classification_span,
            options->moisture_compression_span, options->oasis_drop,
            options->transition_margin);
}
