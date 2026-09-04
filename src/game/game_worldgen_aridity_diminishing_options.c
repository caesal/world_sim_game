#include "game/game_worldgen_aridity_diminishing_options.h"
#include "game/game_worldgen_aridity_diminishing_oasis_histogram.h"
#include "game/game_worldgen_aridity_response_metrics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIM_ACK_ENV "WORLD_SIM_ARIDITY_CALIBRATION_ACK"
#define DIM_ACTION_ENV "WORLD_SIM_ARIDITY_DIMINISHING_ACTION"
#define DIM_BASE_ENV "WORLD_SIM_ARIDITY_DIMINISHING_ARID_BASE"
#define DIM_BIAS_ENV "WORLD_SIM_ARIDITY_DIMINISHING_DESERT_BIAS_SPAN"
#define DIM_DIVISOR_ENV "WORLD_SIM_ARIDITY_DIMINISHING_DROUGHT_DIVISOR"
#define DIM_COMPRESSION_ENV \
    "WORLD_SIM_ARIDITY_DIMINISHING_MOISTURE_COMPRESSION_SPAN"
#define DIM_DROUGHT_SPAN_ENV \
    "WORLD_SIM_ARIDITY_DIMINISHING_DROUGHT_CLASSIFICATION_SPAN"
#define DIM_DROP_ENV "WORLD_SIM_ARIDITY_DIMINISHING_OASIS_DROP"
#define DIM_TRANSITION_ENV "WORLD_SIM_ARIDITY_DIMINISHING_TRANSITION_MARGIN"
#define DIM_ARTIFACT_ENV "WORLD_SIM_ARIDITY_DIMINISHING_EMIT_ARTIFACTS"
#define DIM_ACK "VER037A_ARIDITY_DIMINISHING_RESPONSE"

static int parse_int(const char *text, int *value) {
    char *end = NULL;
    long parsed;
    if (!text || !text[0] || !value) return 0;
    parsed = strtol(text, &end, 10);
    if (!end || *end != '\0' || parsed < 0 || parsed > 100) return 0;
    *value = (int)parsed;
    return 1;
}

int game_worldgen_aridity_diminishing_acknowledgement_matches(
    const char *value) {
    return value && strcmp(value, DIM_ACK) == 0;
}

static int assign_action(
    const char *name, GameWorldgenAridityDiminishingOptions *options) {
    if (strcmp(name, "formula") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_FORMULA;
    } else if (strcmp(name, "baseline") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_BASELINE;
    } else if (strcmp(name, "carriers") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_CARRIERS;
    } else if (strcmp(name, "oasis") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_OASIS;
    } else if (strcmp(name, "confirm") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_CONFIRM;
    } else if (strcmp(name, "holdout") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_HOLDOUT;
    } else if (strcmp(name, "production") == 0) {
        options->action = GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_PRODUCTION;
    } else {
        return 0;
    }
    options->action_name = name;
    return 1;
}

static int all_values_absent(
    const char *base, const char *bias, const char *divisor,
    const char *compression, const char *drought_span, const char *drop,
    const char *transition, const char *artifacts) {
    return !base && !bias && !divisor && !compression && !drought_span &&
        !drop && !transition && !artifacts;
}

static int parse_fixed_candidate(
    GameWorldgenAridityDiminishingOptions *options,
    const char *base, const char *bias, const char *divisor,
    const char *compression, const char *drought_span) {
    GameWorldgenAridityResponseOptions *response =
        &options->projection.response;
    if (!parse_int(base, &response->arid_base) ||
        !parse_int(bias, &response->desert_bias_span) ||
        !parse_int(divisor, &response->drought_divisor) ||
        !parse_int(compression, &response->moisture_compression_span) ||
        !parse_int(drought_span, &response->drought_classification_span)) {
        return 0;
    }
    response->use_override = 1;
    options->projection.has_candidate = 1;
    return game_worldgen_aridity_diminishing_fixed_candidate_matches(options);
}

int game_worldgen_aridity_diminishing_fixed_candidate_matches(
    const GameWorldgenAridityDiminishingOptions *options) {
    const GameWorldgenAridityResponseOptions *response;
    if (!options || !options->projection.has_candidate) return 0;
    response = &options->projection.response;
    return response->use_override && response->arid_base == 31 &&
        response->desert_bias_span == 4 &&
        response->drought_divisor == 24 &&
        response->moisture_compression_span == 12 &&
        response->drought_classification_span == 2;
}

int game_worldgen_aridity_diminishing_options_parse(
    GameWorldgenAridityDiminishingOptions *options) {
    const char *ack = getenv(DIM_ACK_ENV);
    const char *action = getenv(DIM_ACTION_ENV);
    const char *base = getenv(DIM_BASE_ENV);
    const char *bias = getenv(DIM_BIAS_ENV);
    const char *divisor = getenv(DIM_DIVISOR_ENV);
    const char *compression = getenv(DIM_COMPRESSION_ENV);
    const char *drought_span = getenv(DIM_DROUGHT_SPAN_ENV);
    const char *drop = getenv(DIM_DROP_ENV);
    const char *transition = getenv(DIM_TRANSITION_ENV);
    const char *artifacts = getenv(DIM_ARTIFACT_ENV);
    if (!options || !action || !action[0]) return 0;
    memset(options, 0, sizeof(*options));
    if (!assign_action(action, options)) return 0;
    if (options->action == GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_BASELINE ||
        options->action == GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_PRODUCTION) {
        options->production_no_override = options->action ==
            GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_PRODUCTION;
        return !ack && all_values_absent(
            base, bias, divisor, compression, drought_span, drop, transition,
            artifacts);
    }
    if (!game_worldgen_aridity_diminishing_acknowledgement_matches(ack)) {
        return 0;
    }
    if (options->action == GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_FORMULA) {
        return all_values_absent(
            base, bias, divisor, compression, drought_span, drop, transition,
            artifacts);
    }
    if (!parse_fixed_candidate(
            options, base, bias, divisor, compression, drought_span)) return 0;
    options->projection.action_name = options->action_name;
    options->projection.response.action =
        GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_SCREEN;
    options->projection.response.action_name = options->action_name;
    if (options->action ==
            GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_CARRIERS) {
        return !drop && !transition && !artifacts;
    }
    if (!parse_int(drop, &options->projection.response.oasis_drop) ||
        !parse_int(transition,
                   &options->projection.response.transition_margin)) return 0;
    if (!game_worldgen_aridity_diminishing_oasis_pair_valid(
            options->projection.response.oasis_drop,
            options->projection.response.transition_margin)) return 0;
    options->projection.has_oasis_pair = 1;
    if (options->action == GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_OASIS) {
        return !artifacts;
    }
    if (!artifacts || strcmp(artifacts, "1") != 0) return 0;
    options->projection.emit_artifacts = 1;
    options->projection.response.emit_artifacts = 1;
    options->projection.response.action =
        GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_CONFIRM;
    return options->action == GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_CONFIRM ||
        options->action == GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_HOLDOUT;
}

int game_worldgen_aridity_diminishing_pair_id(
    const GameWorldgenAridityDiminishingOptions *options,
    char *out, size_t capacity) {
    int written;
    if (!options || !options->projection.has_oasis_pair || !out ||
        capacity == 0) return 0;
    written = snprintf(out, capacity, "o%02d_t%02d",
        options->projection.response.oasis_drop,
        options->projection.response.transition_margin);
    return written > 0 && (size_t)written < capacity;
}

void game_worldgen_aridity_diminishing_reset_all(void) {
    game_worldgen_aridity_projection_reset_all();
}
