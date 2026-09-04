#include "game/game_worldgen_aridity_response_projection_options.h"
#include "game/game_worldgen_aridity_response_metrics.h"

#include "world/world_gen_aridity_projection.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_moisture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROJECTION_ACK_ENV "WORLD_SIM_ARIDITY_CALIBRATION_ACK"
#define PROJECTION_ACTION_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_ACTION"
#define PROJECTION_ARID_BASE_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_ARID_BASE"
#define PROJECTION_BIAS_SPAN_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_DESERT_BIAS_SPAN"
#define PROJECTION_DROUGHT_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_DROUGHT_DIVISOR"
#define PROJECTION_COMPRESSION_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_MOISTURE_COMPRESSION_SPAN"
#define PROJECTION_OASIS_DROP_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_OASIS_DROP"
#define PROJECTION_TRANSITION_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_TRANSITION_MARGIN"
#define PROJECTION_ARTIFACT_ENV \
    "WORLD_SIM_ARIDITY_RESPONSE_PROJECTION_EMIT_ARTIFACTS"
#define PROJECTION_ACK "VER037A_ARIDITY_RESPONSE_PROJECTION"

static int parse_int(const char *text, int *value) {
    char *end = NULL;
    long parsed;
    if (!text || !text[0] || !value) return 0;
    parsed = strtol(text, &end, 10);
    if (!end || *end != '\0' || parsed < 0 || parsed > 100) return 0;
    *value = (int)parsed;
    return 1;
}

static int value_in(int value, const int *values, int count) {
    int index;
    for (index = 0; index < count; index++) {
        if (values[index] == value) return 1;
    }
    return 0;
}

static int response_candidate_in_grid(
    const GameWorldgenAridityResponseOptions *response) {
    static const int bases[] = {30, 31, 32, 33, 34};
    static const int spans[] = {4, 6, 8, 10, 12, 14};
    static const int divisors[] = {6, 8, 10, 12, 14, 16, 20, 24};
    static const int compressions[] = {10, 12, 14, 16, 18, 20, 22, 24};
    return response &&
        value_in(response->arid_base, bases, 5) &&
        value_in(response->desert_bias_span, spans, 6) &&
        value_in(response->drought_divisor, divisors, 8) &&
        value_in(response->moisture_compression_span, compressions, 8);
}

static int oasis_pair_in_grid(
    const GameWorldgenAridityResponseOptions *response) {
    return response && game_worldgen_aridity_response_oasis_pair_valid(
        response->oasis_drop, response->transition_margin);
}

int game_worldgen_aridity_projection_acknowledgement_matches(
    const char *value) {
    return value && strcmp(value, PROJECTION_ACK) == 0;
}

static int assign_action(
    const char *name, GameWorldgenAridityProjectionOptions *options) {
    if (strcmp(name, "formula") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_FORMULA;
    } else if (strcmp(name, "baseline") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_BASELINE;
    } else if (strcmp(name, "carriers") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CARRIERS;
    } else if (strcmp(name, "oasis") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_OASIS;
    } else if (strcmp(name, "confirm") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CONFIRM;
    } else {
        return 0;
    }
    options->action_name = name;
    return 1;
}

static int candidate_values_absent(
    const char *base, const char *span, const char *divisor,
    const char *compression, const char *drop, const char *transition,
    const char *artifacts) {
    return !base && !span && !divisor && !compression && !drop &&
        !transition && !artifacts;
}

static int parse_candidate(
    GameWorldgenAridityProjectionOptions *options,
    const char *base, const char *span, const char *divisor,
    const char *compression) {
    GameWorldgenAridityResponseOptions *response = &options->response;
    if (!parse_int(base, &response->arid_base) ||
        !parse_int(span, &response->desert_bias_span) ||
        !parse_int(divisor, &response->drought_divisor) ||
        !parse_int(compression, &response->moisture_compression_span) ||
        !response_candidate_in_grid(response)) return 0;
    response->action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_SCREEN;
    response->action_name = options->action_name;
    response->use_override = 1;
    options->has_candidate = 1;
    return 1;
}

int game_worldgen_aridity_projection_options_parse(
    GameWorldgenAridityProjectionOptions *options) {
    const char *ack = getenv(PROJECTION_ACK_ENV);
    const char *action = getenv(PROJECTION_ACTION_ENV);
    const char *base = getenv(PROJECTION_ARID_BASE_ENV);
    const char *span = getenv(PROJECTION_BIAS_SPAN_ENV);
    const char *divisor = getenv(PROJECTION_DROUGHT_ENV);
    const char *compression = getenv(PROJECTION_COMPRESSION_ENV);
    const char *drop = getenv(PROJECTION_OASIS_DROP_ENV);
    const char *transition = getenv(PROJECTION_TRANSITION_ENV);
    const char *artifacts = getenv(PROJECTION_ARTIFACT_ENV);
    if (!options || !action || !action[0]) return 0;
    memset(options, 0, sizeof(*options));
    if (!assign_action(action, options)) return 0;
    if (options->action == GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_BASELINE) {
        return !ack && candidate_values_absent(
            base, span, divisor, compression, drop, transition, artifacts);
    }
    if (!game_worldgen_aridity_projection_acknowledgement_matches(ack)) {
        return 0;
    }
    if (options->action == GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_FORMULA ||
        options->action == GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_CARRIERS) {
        return candidate_values_absent(
            base, span, divisor, compression, drop, transition, artifacts);
    }
    if (!parse_candidate(options, base, span, divisor, compression)) return 0;
    if (options->action == GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_OASIS) {
        if (!parse_int(drop, &options->response.oasis_drop) ||
            !parse_int(transition, &options->response.transition_margin) ||
            !oasis_pair_in_grid(&options->response) || artifacts) return 0;
        options->has_oasis_pair = 1;
        return 1;
    }
    if (!parse_int(drop, &options->response.oasis_drop) ||
        !parse_int(transition, &options->response.transition_margin) ||
        !oasis_pair_in_grid(&options->response) ||
        !artifacts || strcmp(artifacts, "1") != 0) return 0;
    options->response.action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_CONFIRM;
    options->response.emit_artifacts = 1;
    options->has_oasis_pair = 1;
    options->emit_artifacts = 1;
    return 1;
}

int game_worldgen_aridity_projection_candidate_id(
    const GameWorldgenAridityProjectionOptions *options,
    char *out, size_t capacity) {
    int written;
    if (!options || !options->has_candidate || !out || capacity == 0) return 0;
    written = snprintf(out, capacity, "a%d_b%02d_d%02d_c%02d",
        options->response.arid_base, options->response.desert_bias_span,
        options->response.drought_divisor,
        options->response.moisture_compression_span);
    return written > 0 && (size_t)written < capacity;
}

int game_worldgen_aridity_projection_pair_id(
    const GameWorldgenAridityProjectionOptions *options,
    char *out, size_t capacity) {
    int written;
    if (!options || !options->has_oasis_pair || !out || capacity == 0) return 0;
    written = snprintf(out, capacity, "o%02d_t%02d",
        options->response.oasis_drop, options->response.transition_margin);
    return written > 0 && (size_t)written < capacity;
}

void game_worldgen_aridity_projection_reset_all(void) {
    world_gen_aridity_projection_validation_reset();
    game_worldgen_aridity_response_reset_overrides();
}

int game_worldgen_aridity_projection_capture_defaults(
    GameWorldgenAridityResponseDefaults *defaults) {
    game_worldgen_aridity_projection_reset_all();
    return game_worldgen_aridity_response_capture_defaults(defaults) &&
        !world_gen_aridity_projection_validation_active();
}

int game_worldgen_aridity_projection_defaults_restored(
    const GameWorldgenAridityResponseDefaults *defaults) {
    return game_worldgen_aridity_response_defaults_restored(defaults) &&
        !world_gen_aridity_projection_validation_active();
}

int game_worldgen_aridity_projection_enable_carrier(int drought_divisor) {
    game_worldgen_aridity_projection_reset_all();
    if (!world_gen_moisture_validation_set_drought_divisor(drought_divisor) ||
        !world_gen_aridity_projection_validation_enable() ||
        !game_worldgen_aridity_projection_carrier_matches(drought_divisor)) {
        game_worldgen_aridity_projection_reset_all();
        return 0;
    }
    return 1;
}

int game_worldgen_aridity_projection_carrier_matches(int drought_divisor) {
    return world_gen_moisture_validation_drought_divisor_active() &&
        world_gen_moisture_drought_divisor() == drought_divisor &&
        !world_gen_classify_validation_aridity_active() &&
        !world_gen_aridity_response_validation_active() &&
        world_gen_aridity_projection_validation_active() &&
        world_gen_aridity_projection_validation_matches();
}

int game_worldgen_aridity_projection_enable_actual(
    const GameWorldgenAridityProjectionOptions *options) {
    game_worldgen_aridity_projection_reset_all();
    if (!options || !options->has_candidate ||
        !game_worldgen_aridity_response_enable_override(&options->response) ||
        !world_gen_aridity_projection_validation_enable() ||
        !game_worldgen_aridity_projection_actual_matches(options)) {
        game_worldgen_aridity_projection_reset_all();
        return 0;
    }
    return 1;
}

int game_worldgen_aridity_projection_actual_matches(
    const GameWorldgenAridityProjectionOptions *options) {
    return options && options->has_candidate &&
        game_worldgen_aridity_response_override_matches(&options->response) &&
        world_gen_aridity_projection_validation_active() &&
        world_gen_aridity_projection_validation_matches();
}
