#include "game/game_worldgen_aridity_calibration_options.h"

#include "world/world_gen_classify.h"
#include "world/world_gen_moisture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALIBRATION_ACK_ENV "WORLD_SIM_ARIDITY_CALIBRATION_ACK"
#define CALIBRATION_STAGE_ENV "WORLD_SIM_ARIDITY_CALIBRATION_STAGE"
#define CALIBRATION_MODE_ENV "WORLD_SIM_ARIDITY_CALIBRATION_MODE"
#define CALIBRATION_DROUGHT_ENV \
    "WORLD_SIM_ARIDITY_CALIBRATION_DROUGHT_DIVISOR"
#define CALIBRATION_BASE_ENV \
    "WORLD_SIM_ARIDITY_CALIBRATION_DESERT_BASE"
#define CALIBRATION_BIAS_ENV \
    "WORLD_SIM_ARIDITY_CALIBRATION_DESERT_BIAS_SPAN"
#define CALIBRATION_WIDTH_ENV \
    "WORLD_SIM_ARIDITY_CALIBRATION_SEMI_ARID_WIDTH"
#define CALIBRATION_MARGIN_ENV \
    "WORLD_SIM_ARIDITY_CALIBRATION_OASIS_TRANSITION_MARGIN"
#define CALIBRATION_ACK "VER037A_ARIDITY_RECALIBRATION"

static const uint32_t QUALIFICATION_SEEDS[] = {
    2026072301u, 2026072302u
};
static const uint32_t SCREENING_SEEDS[] = {2026072301u};
static const uint32_t CALIBRATION_SEEDS[] = {
    2026072302u, 2026072303u, 2026072304u, 2026072305u
};
static const uint32_t HOLDOUT_SEEDS[] = {
    2026082201u, 2026082202u, 2026082203u, 2026082204u
};
static const uint32_t FINAL_SEEDS[] = {
    2026072301u,
    2026072302u, 2026072303u, 2026072304u, 2026072305u,
    2026082201u, 2026082202u, 2026082203u, 2026082204u
};

static int parse_int_range(const char *text, int low, int high, int *value) {
    char *end = NULL;
    long parsed;
    if (!text || !text[0] || !value) return 0;
    parsed = strtol(text, &end, 10);
    if (!end || *end != '\0' || parsed < low || parsed > high) return 0;
    *value = (int)parsed;
    return 1;
}

static int in_values(int value, const int *values, int count) {
    int index;
    for (index = 0; index < count; index++) {
        if (value == values[index]) return 1;
    }
    return 0;
}

static int candidate_in_grid(
    int drought_divisor, int desert_base, int desert_bias_span,
    int semi_arid_width) {
    static const int drought_values[] = {16, 20, 24, 32};
    static const int base_values[] = {8, 10, 12, 14};
    static const int bias_values[] = {2, 4, 6, 8};
    static const int width_values[] = {8, 10, 12, 14};
    return in_values(drought_divisor, drought_values, 4) &&
        in_values(desert_base, base_values, 4) &&
        in_values(desert_bias_span, bias_values, 4) &&
        in_values(semi_arid_width, width_values, 4);
}

static int margin_in_grid(int margin) {
    static const int margins[] = {0, 5, 10, 15, 20, 25, 30};
    return in_values(margin, margins, 7);
}

static int assign_stage(
    const char *name, GameWorldgenAridityCalibrationOptions *options) {
    if (strcmp(name, "qualification") == 0) {
        options->stage = GAME_WORLDGEN_ARIDITY_STAGE_QUALIFICATION;
        options->seeds = QUALIFICATION_SEEDS;
        options->seed_count = 2;
    } else if (strcmp(name, "pilot") == 0) {
        options->stage = GAME_WORLDGEN_ARIDITY_STAGE_PILOT;
        options->seeds = SCREENING_SEEDS;
        options->seed_count = 1;
    } else if (strcmp(name, "screening") == 0) {
        options->stage = GAME_WORLDGEN_ARIDITY_STAGE_SCREENING;
        options->seeds = SCREENING_SEEDS;
        options->seed_count = 1;
    } else if (strcmp(name, "calibration") == 0) {
        options->stage = GAME_WORLDGEN_ARIDITY_STAGE_CALIBRATION;
        options->seeds = CALIBRATION_SEEDS;
        options->seed_count = 4;
    } else if (strcmp(name, "confirmation") == 0) {
        options->stage = GAME_WORLDGEN_ARIDITY_STAGE_CONFIRMATION;
        options->seeds = CALIBRATION_SEEDS;
        options->seed_count = 4;
    } else if (strcmp(name, "holdout") == 0) {
        options->stage = GAME_WORLDGEN_ARIDITY_STAGE_HOLDOUT;
        options->seeds = HOLDOUT_SEEDS;
        options->seed_count = 4;
    } else if (strcmp(name, "final") == 0) {
        options->stage = GAME_WORLDGEN_ARIDITY_STAGE_FINAL;
        options->seeds = FINAL_SEEDS;
        options->seed_count = 9;
    } else {
        return 0;
    }
    options->stage_name = name;
    return 1;
}

int game_worldgen_aridity_calibration_acknowledgement_matches(
    const char *value) {
    return value && strcmp(value, CALIBRATION_ACK) == 0;
}

int game_worldgen_aridity_calibration_requested(void) {
    return game_worldgen_aridity_calibration_acknowledgement_matches(
        getenv(CALIBRATION_ACK_ENV));
}

int game_worldgen_aridity_calibration_options_parse(
    GameWorldgenAridityCalibrationOptions *options) {
    const char *ack = getenv(CALIBRATION_ACK_ENV);
    const char *stage = getenv(CALIBRATION_STAGE_ENV);
    const char *mode = getenv(CALIBRATION_MODE_ENV);
    const char *drought = getenv(CALIBRATION_DROUGHT_ENV);
    const char *base = getenv(CALIBRATION_BASE_ENV);
    const char *bias = getenv(CALIBRATION_BIAS_ENV);
    const char *width = getenv(CALIBRATION_WIDTH_ENV);
    const char *margin = getenv(CALIBRATION_MARGIN_ENV);
    if (!options ||
        !game_worldgen_aridity_calibration_acknowledgement_matches(ack) ||
        !stage || !mode) return 0;
    memset(options, 0, sizeof(*options));
    if (!assign_stage(stage, options)) return 0;
    options->mode_name = mode;
    if (strcmp(mode, "production") == 0) {
        if (drought || base || bias || width || margin ||
            (options->stage != GAME_WORLDGEN_ARIDITY_STAGE_QUALIFICATION &&
             options->stage != GAME_WORLDGEN_ARIDITY_STAGE_FINAL)) return 0;
        options->drought_divisor = world_gen_moisture_drought_divisor();
        options->desert_base = world_gen_desert_base();
        options->desert_bias_span = world_gen_desert_bias_span();
        options->semi_arid_width = world_gen_semi_arid_width();
        options->oasis_transition_margin =
            world_gen_oasis_transition_margin();
        return options->stage != GAME_WORLDGEN_ARIDITY_STAGE_QUALIFICATION ||
            (options->drought_divisor == 4 &&
             options->desert_base == 14 &&
             options->desert_bias_span == 22 &&
             options->semi_arid_width == 16 &&
             options->oasis_transition_margin == 0);
    }
    if (strcmp(mode, "override") != 0 ||
        !parse_int_range(drought, 1, 100, &options->drought_divisor) ||
        !parse_int_range(base, 1, 100, &options->desert_base) ||
        !parse_int_range(bias, 1, 100, &options->desert_bias_span) ||
        !parse_int_range(width, 1, 100, &options->semi_arid_width) ||
        !parse_int_range(margin, 0, 100,
                         &options->oasis_transition_margin) ||
        options->stage == GAME_WORLDGEN_ARIDITY_STAGE_FINAL) return 0;
    options->use_override = 1;
    if (options->stage == GAME_WORLDGEN_ARIDITY_STAGE_QUALIFICATION) {
        return options->drought_divisor == 4 &&
            options->desert_base == 14 &&
            options->desert_bias_span == 22 &&
            options->semi_arid_width == 16 &&
            options->oasis_transition_margin == 0;
    }
    if (!candidate_in_grid(
            options->drought_divisor, options->desert_base,
            options->desert_bias_span, options->semi_arid_width) ||
        !margin_in_grid(options->oasis_transition_margin)) return 0;
    if (options->stage == GAME_WORLDGEN_ARIDITY_STAGE_PILOT &&
        (options->drought_divisor != 24 || options->desert_base != 10 ||
         options->desert_bias_span != 4 ||
         options->semi_arid_width != 10 ||
         options->oasis_transition_margin != 0)) return 0;
    if ((options->stage == GAME_WORLDGEN_ARIDITY_STAGE_SCREENING ||
         options->stage == GAME_WORLDGEN_ARIDITY_STAGE_CALIBRATION) &&
        options->oasis_transition_margin != 0) return 0;
    return 1;
}

int game_worldgen_aridity_calibration_candidate_id(
    const GameWorldgenAridityCalibrationOptions *options,
    char *out, size_t capacity) {
    int written;
    if (!options || !out || capacity == 0) return 0;
    written = snprintf(out, capacity, "d%d_a%d_b%d_s%d",
                       options->drought_divisor, options->desert_base,
                       options->desert_bias_span, options->semi_arid_width);
    return written > 0 && (size_t)written < capacity;
}

void game_worldgen_aridity_calibration_reset_overrides(void) {
    world_gen_moisture_validation_reset_drought_divisor();
    world_gen_classify_validation_reset_aridity();
}

int game_worldgen_aridity_calibration_capture_defaults(
    GameWorldgenAridityCalibrationDefaults *defaults) {
    if (!defaults) return 0;
    game_worldgen_aridity_calibration_reset_overrides();
    defaults->drought_divisor = world_gen_moisture_drought_divisor();
    defaults->desert_base = world_gen_desert_base();
    defaults->desert_bias_span = world_gen_desert_bias_span();
    defaults->semi_arid_width = world_gen_semi_arid_width();
    defaults->oasis_transition_margin = world_gen_oasis_transition_margin();
    return game_worldgen_aridity_calibration_defaults_restored(defaults);
}

int game_worldgen_aridity_calibration_defaults_restored(
    const GameWorldgenAridityCalibrationDefaults *defaults) {
    return defaults &&
        !world_gen_moisture_validation_drought_divisor_active() &&
        !world_gen_classify_validation_aridity_active() &&
        world_gen_moisture_drought_divisor() == defaults->drought_divisor &&
        world_gen_desert_base() == defaults->desert_base &&
        world_gen_desert_bias_span() == defaults->desert_bias_span &&
        world_gen_semi_arid_width() == defaults->semi_arid_width &&
        world_gen_oasis_transition_margin() ==
            defaults->oasis_transition_margin;
}

int game_worldgen_aridity_calibration_enable_override(
    const GameWorldgenAridityCalibrationOptions *options) {
    game_worldgen_aridity_calibration_reset_overrides();
    if (!options || !options->use_override) return 0;
    if (!world_gen_moisture_validation_set_drought_divisor(
            options->drought_divisor) ||
        !world_gen_classify_validation_set_aridity(
            options->desert_base, options->desert_bias_span,
            options->semi_arid_width, options->oasis_transition_margin)) {
        game_worldgen_aridity_calibration_reset_overrides();
        return 0;
    }
    if (!game_worldgen_aridity_calibration_override_matches(options)) {
        game_worldgen_aridity_calibration_reset_overrides();
        return 0;
    }
    return 1;
}

int game_worldgen_aridity_calibration_override_matches(
    const GameWorldgenAridityCalibrationOptions *options) {
    return options && options->use_override &&
        world_gen_moisture_validation_drought_divisor_active() &&
        world_gen_classify_validation_aridity_active() &&
        world_gen_moisture_drought_divisor() == options->drought_divisor &&
        world_gen_desert_base() == options->desert_base &&
        world_gen_desert_bias_span() == options->desert_bias_span &&
        world_gen_semi_arid_width() == options->semi_arid_width &&
        world_gen_oasis_transition_margin() ==
            options->oasis_transition_margin;
}
