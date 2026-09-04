#include "game/game_worldgen_aridity_diminishing_probe.h"

#include "game/game_worldgen_aridity_calibration_metrics.h"
#include "game/game_worldgen_aridity_diminishing_matrix.h"
#include "game/game_worldgen_aridity_diminishing_oasis_histogram.h"
#include "game/game_worldgen_aridity_diminishing_options.h"
#include "game/game_worldgen_aridity_response_metrics.h"
#include "game/game_worldgen_aridity_response_options.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_moisture.h"

#include <stdio.h>
#include <string.h>

enum {
    DIM_ARID_BASE = 31,
    DIM_BIAS_SPAN = 4,
    DIM_DROUGHT_DIVISOR = 24,
    DIM_COMPRESSION = 12,
    DIM_DROUGHT_SPAN = 2
};

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int acknowledgement_matrix_ok(int *case_count) {
    static const char *const values[] = {
        NULL,
        "",
        "ver037a_aridity_diminishing_response",
        "VER037A_ARIDITY_DIMINISHING_RESPONSE_extra",
        "prefix_VER037A_ARIDITY_DIMINISHING_RESPONSE",
        "VER037A_ARIDITY_RESPONSE_PROJECTION",
        "VER037A_ARIDITY_RESPONSE_PILOT",
        "arbitrary-nonempty",
        "VER037A_ARIDITY_DIMINISHING_RESPONSE"
    };
    static const int expected[] = {0, 0, 0, 0, 0, 0, 0, 0, 1};
    int index;
    int ok = 1;
    *case_count = 0;
    for (index = 0; index < 9; index++) {
        ok &= game_worldgen_aridity_diminishing_acknowledgement_matches(
            values[index]) == expected[index];
        (*case_count)++;
    }
    return ok;
}

static int expected_dryness_response(int drought, int bias_desert) {
    int bias_numerator = bias_desert * DIM_BIAS_SPAN * 100;
    int drought_numerator = drought * DIM_DROUGHT_SPAN *
        (100 - bias_desert);
    return (bias_numerator + drought_numerator) / 10000;
}

static int response_grid_ok(
    int *formula_cases, int *bias_monotonic_cases,
    int *drought_monotonic_cases) {
    int drought;
    int bias;
    int ok = 1;
    *formula_cases = 0;
    *bias_monotonic_cases = 0;
    *drought_monotonic_cases = 0;
    for (drought = 0; drought <= 100; drought++) {
        int previous = -1;
        for (bias = 0; bias <= 100; bias++) {
            WorldGenAridityResponseLimits limits;
            int expected = expected_dryness_response(drought, bias);
            ok &= world_gen_aridity_response_calculate_diminishing(
                50, drought, bias, DIM_ARID_BASE, DIM_BIAS_SPAN,
                DIM_DROUGHT_SPAN, DIM_COMPRESSION, 8, 0, &limits);
            ok &= limits.combined_arid_limit == DIM_ARID_BASE + expected;
            ok &= expected >= previous;
            previous = expected;
            (*formula_cases)++;
            if (bias > 0) (*bias_monotonic_cases)++;
        }
    }
    for (bias = 0; bias <= 100; bias++) {
        int previous = -1;
        for (drought = 0; drought <= 100; drought++) {
            int expected = expected_dryness_response(drought, bias);
            ok &= expected >= previous;
            previous = expected;
            if (drought > 0) (*drought_monotonic_cases)++;
        }
    }
    return ok && *formula_cases == 10201 &&
        *bias_monotonic_cases == 10100 &&
        *drought_monotonic_cases == 10100;
}

static int endpoint_ok(int *case_count) {
    static const int droughts[] = {0, 0, 100, 100};
    static const int biases[] = {0, 100, 0, 100};
    static const int expected[] = {0, 4, 2, 4};
    int index;
    int ok = 1;
    *case_count = 0;
    for (index = 0; index < 4; index++) {
        ok &= expected_dryness_response(droughts[index], biases[index]) ==
            expected[index];
        (*case_count)++;
    }
    return ok;
}

static int matrix_formula_ok(int *case_count) {
    int case_index;
    int ok = 1;
    *case_count = 0;
    for (case_index = 0;
         case_index < GAME_WORLDGEN_ARIDITY_CASE_COUNT; case_index++) {
        const GameWorldgenAridityCase *matrix_case =
            &game_worldgen_aridity_cases[case_index];
        WorldGenAridityResponseLimits limits;
        int below_50 = matrix_case->moisture < 50 ?
            50 - matrix_case->moisture : 0;
        int response = expected_dryness_response(
            matrix_case->drought, matrix_case->bias_desert);
        int combined = clamp_int(
            DIM_ARID_BASE + response +
                (matrix_case->moisture - 50) * DIM_COMPRESSION / 25,
            0, 100);
        int band = clamp_int(
            12 - matrix_case->bias_desert * 10 / 100 + below_50 * 8 / 25,
            2, 20);
        ok &= world_gen_aridity_response_calculate_diminishing(
            matrix_case->moisture, matrix_case->drought,
            matrix_case->bias_desert, DIM_ARID_BASE, DIM_BIAS_SPAN,
            DIM_DROUGHT_SPAN, DIM_COMPRESSION, 8, 0, &limits);
        ok &= limits.combined_arid_limit == combined &&
            limits.semi_arid_band == band &&
            limits.desert_limit == clamp_int(combined - band, 0, 100) &&
            limits.semi_arid_limit == combined &&
            limits.oasis_limit == 42 - matrix_case->drought * 8 / 100 &&
            limits.oasis_transition_limit == combined;
        (*case_count)++;
    }
    return ok && *case_count == 6;
}

static int base_air_ok(int *case_count) {
    static const int moistures[] = {0, 50, 100};
    static const int droughts[] = {0, 100};
    static const int noises[] = {-7, 0, 7};
    int moisture_index;
    int drought_index;
    int noise_index;
    int ok = 1;
    *case_count = 0;
    for (moisture_index = 0; moisture_index < 3; moisture_index++) {
        for (drought_index = 0; drought_index < 2; drought_index++) {
            for (noise_index = 0; noise_index < 3; noise_index++) {
                int actual = -1;
                int expected = clamp_int(
                    18 + moistures[moisture_index] / 2 -
                        droughts[drought_index] / DIM_DROUGHT_DIVISOR +
                        noises[noise_index] / 3,
                    4, 82);
                ok &= world_gen_moisture_base_air_calculate(
                    moistures[moisture_index], droughts[drought_index],
                    DIM_DROUGHT_DIVISOR, noises[noise_index], &actual) &&
                    actual == expected;
                (*case_count)++;
            }
        }
    }
    return ok && *case_count == 18;
}

static void fixed_response(GameWorldgenAridityResponseOptions *response) {
    memset(response, 0, sizeof(*response));
    response->action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_SCREEN;
    response->action_name = "formula";
    response->use_override = 1;
    response->arid_base = DIM_ARID_BASE;
    response->desert_bias_span = DIM_BIAS_SPAN;
    response->drought_divisor = DIM_DROUGHT_DIVISOR;
    response->moisture_compression_span = DIM_COMPRESSION;
    response->drought_classification_span = DIM_DROUGHT_SPAN;
    response->oasis_drop = 8;
    response->transition_margin = 0;
}

static int oasis_grid_ok(int *case_count) {
    static const int expected_drops[] = {0, 4, 8, 12, 16, 20};
    static const int expected_margins[] = {0, 5, 10, 15, 20, 25};
    static const int invalid_drops[] = {-1, 1, 3, 21};
    static const int invalid_margins[] = {-1, 1, 4, 24, 26};
    GameWorldgenAridityDiminishingOptions options;
    int drop_index;
    int margin_index;
    int ok = 1;
    *case_count = 0;
    memset(&options, 0, sizeof(options));
    options.projection.has_oasis_pair = 1;
    if (GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT != 6 ||
        GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT != 6) return 0;
    for (drop_index = 0;
         drop_index < GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT;
        drop_index++) {
        ok &= game_worldgen_aridity_response_oasis_drops[drop_index] ==
            expected_drops[drop_index];
        ok &= game_worldgen_aridity_response_oasis_drop_index(
            expected_drops[drop_index]) == drop_index;
        (*case_count)++;
    }
    for (margin_index = 0;
         margin_index < GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT;
         margin_index++) {
        ok &= game_worldgen_aridity_response_transition_margins[
            margin_index] == expected_margins[margin_index];
        ok &= game_worldgen_aridity_response_transition_margin_index(
            expected_margins[margin_index]) == margin_index;
        (*case_count)++;
    }
    for (drop_index = 0;
         drop_index < GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT;
         drop_index++) {
        for (margin_index = 0;
             margin_index < GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT;
             margin_index++) {
            WorldGenAridityResponseLimits limits;
            char actual_id[16];
            char expected_id[16];
            int drop = game_worldgen_aridity_response_oasis_drops[drop_index];
            int margin = game_worldgen_aridity_response_transition_margins[
                margin_index];
            options.projection.response.oasis_drop = drop;
            options.projection.response.transition_margin = margin;
            ok &= world_gen_aridity_response_calculate_diminishing(
                50, 100, 100, DIM_ARID_BASE, DIM_BIAS_SPAN,
                DIM_DROUGHT_SPAN, DIM_COMPRESSION, drop, margin, &limits);
            ok &= game_worldgen_aridity_response_oasis_pair_valid(
                drop, margin);
            ok &= snprintf(expected_id, sizeof(expected_id), "o%02d_t%02d",
                    drop, margin) > 0 &&
                game_worldgen_aridity_diminishing_pair_id(
                    &options, actual_id, sizeof(actual_id)) &&
                strcmp(actual_id, expected_id) == 0;
            ok &= limits.oasis_limit == 42 - drop;
            ok &= limits.oasis_transition_limit == 35 + margin;
            (*case_count)++;
        }
    }
    for (drop_index = 0;
         drop_index < (int)(sizeof(invalid_drops) / sizeof(invalid_drops[0]));
         drop_index++) {
        ok &= game_worldgen_aridity_response_oasis_drop_index(
            invalid_drops[drop_index]) < 0;
        ok &= !game_worldgen_aridity_response_oasis_pair_valid(
            invalid_drops[drop_index], 0);
        (*case_count)++;
    }
    for (margin_index = 0;
         margin_index <
             (int)(sizeof(invalid_margins) / sizeof(invalid_margins[0]));
         margin_index++) {
        ok &= game_worldgen_aridity_response_transition_margin_index(
            invalid_margins[margin_index]) < 0;
        ok &= !game_worldgen_aridity_response_oasis_pair_valid(
            0, invalid_margins[margin_index]);
        (*case_count)++;
    }
    return ok && *case_count == 57;
}

static int strict_oasis_window_ok(int *case_count) {
    GameWorldgenAridityResponseOptions response;
    WorldGenContext context;
    int16_t moisture = 0;
    int16_t temperature = 0;
    uint8_t land = 1;
    uint16_t river = WORLD_GEN_RIVER_CHANNEL;
    int ok;
    memset(&context, 0, sizeof(context));
    context.width = 1;
    context.height = 1;
    context.tile_count = 1;
    context.config.moisture = 50;
    context.config.drought = 100;
    context.config.bias_desert = 100;
    context.moisture = &moisture;
    context.temperature = &temperature;
    context.land_mask = &land;
    context.river_flags = &river;
    fixed_response(&response);
    response.transition_margin = 5;
    game_worldgen_aridity_diminishing_reset_all();
    ok = game_worldgen_aridity_response_enable_override(&response);
    *case_count = 0;

    moisture = 34;
    temperature = 29;
    ok &= !world_gen_classify_response_oasis_predicate_for_pair(
        &context, 0, CLIMATE_DESERT, 8, 5);
    (*case_count)++;
    moisture = 35;
    ok &= world_gen_classify_response_oasis_predicate_for_pair(
        &context, 0, CLIMATE_DESERT, 8, 5);
    (*case_count)++;
    moisture = 40;
    ok &= !world_gen_classify_response_oasis_predicate_for_pair(
        &context, 0, CLIMATE_DESERT, 8, 5);
    (*case_count)++;
    moisture = 35;
    temperature = 28;
    ok &= !world_gen_classify_response_oasis_predicate_for_pair(
        &context, 0, CLIMATE_CONTINENTAL, 8, 5);
    (*case_count)++;
    temperature = 29;
    ok &= world_gen_classify_response_oasis_predicate_for_pair(
        &context, 0, CLIMATE_CONTINENTAL, 8, 5);
    (*case_count)++;
    river = 0;
    ok &= !world_gen_classify_response_oasis_predicate_for_pair(
        &context, 0, CLIMATE_DESERT, 8, 5);
    (*case_count)++;

    river = WORLD_GEN_RIVER_CHANNEL;
    context.config.drought = 0;
    moisture = 43;
    ok &= world_gen_classify_response_oasis_predicate_for_pair(
        &context, 0, CLIMATE_DESERT, 8, 5);
    (*case_count)++;
    ok &= !world_gen_classify_response_oasis_predicate_for_pair(
        &context, 0, CLIMATE_CONTINENTAL, 8, 5);
    (*case_count)++;

    game_worldgen_aridity_diminishing_reset_all();
    context.config.drought = 100;
    moisture = 50;
    ok &= world_gen_classify_oasis_predicate_for_margin(
        &context, 0, CLIMATE_DESERT, 0);
    (*case_count)++;
    game_worldgen_aridity_diminishing_reset_all();
    return ok && *case_count == 9;
}

static int lifecycle_ok(int *case_count) {
    GameWorldgenAridityResponseDefaults defaults;
    GameWorldgenAridityResponseOptions response;
    int ok;
    *case_count = 0;
    fixed_response(&response);
    game_worldgen_aridity_diminishing_reset_all();
    ok = game_worldgen_aridity_response_capture_defaults(&defaults) &&
        game_worldgen_aridity_response_defaults_restored(&defaults);
    (*case_count)++;
    ok &= game_worldgen_aridity_response_enable_override(&response) &&
        game_worldgen_aridity_response_override_matches(&response);
    (*case_count)++;
    game_worldgen_aridity_diminishing_reset_all();
    ok &= game_worldgen_aridity_response_defaults_restored(&defaults);
    (*case_count)++;
    return ok && *case_count == 3;
}

static int production_defaults_ok(int *case_count) {
    WorldGenAridityResponseLimits limits;
    int ok;
    game_worldgen_aridity_diminishing_reset_all();
    ok = !world_gen_aridity_response_validation_active() &&
        !world_gen_moisture_validation_drought_divisor_active() &&
        !world_gen_classify_validation_aridity_active() &&
        world_gen_moisture_drought_divisor() == DIM_DROUGHT_DIVISOR &&
        world_gen_aridity_response_arid_base() == DIM_ARID_BASE &&
        world_gen_aridity_response_desert_bias_span() == DIM_BIAS_SPAN &&
        world_gen_aridity_response_drought_classification_span() ==
            DIM_DROUGHT_SPAN &&
        world_gen_aridity_response_moisture_compression_span() ==
            DIM_COMPRESSION &&
        world_gen_aridity_response_oasis_drop() == 20 &&
        world_gen_aridity_response_transition_margin() == 2 &&
        world_gen_aridity_response_current_limits(50, 100, 100, &limits) &&
        limits.combined_arid_limit == 35 && limits.semi_arid_band == 2 &&
        limits.desert_limit == 33 && limits.semi_arid_limit == 35 &&
        limits.oasis_limit == 22 && limits.oasis_transition_limit == 37;
    *case_count = 1;
    return ok;
}

static int run_formula_probe(void) {
    GameWorldgenAridityDiminishingOasisHistogramSelfTest histogram_test;
    int ack_cases = 0;
    int formula_cases = 0;
    int bias_monotonic_cases = 0;
    int drought_monotonic_cases = 0;
    int endpoint_cases = 0;
    int matrix_cases = 0;
    int base_air_cases = 0;
    int oasis_grid_cases = 0;
    int oasis_window_cases = 0;
    int lifecycle_cases = 0;
    int production_default_cases = 0;
    int ack_ok = acknowledgement_matrix_ok(&ack_cases);
    int formula_ok = response_grid_ok(
        &formula_cases, &bias_monotonic_cases, &drought_monotonic_cases);
    int endpoints_ok = endpoint_ok(&endpoint_cases);
    int cases_ok = matrix_formula_ok(&matrix_cases);
    int air_ok = base_air_ok(&base_air_cases);
    int oasis_grid_pass = oasis_grid_ok(&oasis_grid_cases);
    int oasis_window_pass = strict_oasis_window_ok(&oasis_window_cases);
    int state_ok = lifecycle_ok(&lifecycle_cases);
    int production_ok = production_defaults_ok(&production_default_cases);
    int histogram_ok =
        game_worldgen_aridity_diminishing_oasis_histogram_self_test(
            &histogram_test);
    int overall_ok = ack_ok && formula_ok && endpoints_ok && cases_ok &&
        air_ok && oasis_grid_pass && oasis_window_pass && state_ok &&
        production_ok && histogram_ok;
    printf("case=aridity_diminishing_formula ack_cases=%d ack_ok=%d "
           "formula_cases=%d bias_monotonic_cases=%d "
           "drought_monotonic_cases=%d formula_ok=%d endpoint_cases=%d "
           "endpoints_ok=%d matrix_cases=%d matrix_ok=%d base_air_cases=%d "
           "base_air_ok=%d oasis_grid_cases=%d oasis_grid_ok=%d "
           "oasis_window_cases=%d oasis_window_ok=%d "
           "lifecycle_cases=%d lifecycle_ok=%d "
           "production_default_cases=%d production_defaults_ok=%d "
           "integer_pair_cases=%d histogram_oracle_cases=%d "
           "histogram_endpoint_cases=%d histogram_boundary_cases=%d "
           "histogram_mismatches=%d histogram_ok=%d "
           "worlds_generated=0 overall_ok=%d\n",
           ack_cases, ack_ok, formula_cases, bias_monotonic_cases,
           drought_monotonic_cases, formula_ok, endpoint_cases, endpoints_ok,
           matrix_cases, cases_ok, base_air_cases, air_ok, oasis_grid_cases,
           oasis_grid_pass, oasis_window_cases, oasis_window_pass,
           lifecycle_cases, state_ok, production_default_cases, production_ok,
           histogram_test.pair_cases,
           histogram_test.oracle_cases, histogram_test.strict_endpoint_cases,
           histogram_test.invalid_boundary_cases, histogram_test.mismatch_count,
           histogram_ok, overall_ok);
    return overall_ok ? 0 : 1;
}

int run_worldgen_aridity_diminishing_response_probe(void) {
    GameWorldgenAridityDiminishingOptions options;
    int result;
    game_worldgen_aridity_diminishing_reset_all();
    if (!game_worldgen_aridity_diminishing_options_parse(&options)) {
        printf("case=aridity_diminishing_options parse_ok=0 "
               "worlds_generated=0 overall_ok=0\n");
        game_worldgen_aridity_diminishing_reset_all();
        return 1;
    }
    result = options.action ==
        GAME_WORLDGEN_ARIDITY_DIMINISHING_ACTION_FORMULA ?
        run_formula_probe() :
        game_worldgen_aridity_diminishing_matrix_run(&options);
    game_worldgen_aridity_diminishing_reset_all();
    return result;
}
