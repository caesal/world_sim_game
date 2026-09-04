#include "game/game_worldgen_aridity_response_projection_probe.h"

#include "game/game_worldgen_aridity_calibration_metrics.h"
#include "game/game_worldgen_aridity_response_metrics.h"
#include "game/game_worldgen_aridity_response_projection_matrix.h"
#include "game/game_worldgen_aridity_response_projection_options.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_moisture.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const int BASES[] = {30, 31, 32, 33, 34};
static const int BIAS_SPANS[] = {4, 6, 8, 10, 12, 14};
static const int DIVISORS[] = {6, 8, 10, 12, 14, 16, 20, 24};
static const int COMPRESSIONS[] = {10, 12, 14, 16, 18, 20, 22, 24};

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int acknowledgement_matrix_ok(int *case_count) {
    static const char *const values[] = {
        NULL,
        "",
        "ver037a_aridity_response_projection",
        "VER037A_ARIDITY_RESPONSE_PROJECTION_extra",
        "prefix_VER037A_ARIDITY_RESPONSE_PROJECTION",
        "VER037A_ARIDITY_RESPONSE_PILOT",
        "VER037A_ARIDITY_RECALIBRATION",
        "arbitrary-nonempty",
        "VER037A_ARIDITY_RESPONSE_PROJECTION"
    };
    static const int expected[] = {0, 0, 0, 0, 0, 0, 0, 0, 1};
    int index;
    int ok = 1;
    if (case_count) *case_count = 0;
    for (index = 0; index < 9; index++) {
        ok &= game_worldgen_aridity_projection_acknowledgement_matches(
            values[index]) == expected[index];
        if (case_count) (*case_count)++;
    }
    return ok;
}

static int candidate_and_pair_grid_ok(
    int *candidate_count, int *pair_count, uint64_t *id_hash) {
    GameWorldgenAridityProjectionOptions options;
    char id[64];
    int base_index;
    int span_index;
    int divisor_index;
    int compression_index;
    int drop_index;
    int transition_index;
    int ok = 1;
    *candidate_count = 0;
    *pair_count = 0;
    *id_hash = UINT64_C(1469598103934665603);
    memset(&options, 0, sizeof(options));
    options.has_candidate = 1;
    for (base_index = 0; base_index < 5; base_index++) {
        for (span_index = 0; span_index < 6; span_index++) {
            for (divisor_index = 0; divisor_index < 8; divisor_index++) {
                for (compression_index = 0; compression_index < 8;
                     compression_index++) {
                    const char *expected_first =
                        "a30_b04_d06_c10";
                    const char *expected_last =
                        "a34_b14_d24_c24";
                    options.response.arid_base = BASES[base_index];
                    options.response.desert_bias_span =
                        BIAS_SPANS[span_index];
                    options.response.drought_divisor =
                        DIVISORS[divisor_index];
                    options.response.moisture_compression_span =
                        COMPRESSIONS[compression_index];
                    ok &= game_worldgen_aridity_projection_candidate_id(
                        &options, id, sizeof(id));
                    if (*candidate_count == 0) {
                        ok &= strcmp(id, expected_first) == 0;
                    }
                    if (*candidate_count == 1919) {
                        ok &= strcmp(id, expected_last) == 0;
                    }
                    {
                        const unsigned char *cursor =
                            (const unsigned char *)id;
                        while (*cursor) {
                            *id_hash = game_worldgen_aridity_hash_mix(
                                *id_hash, *cursor++);
                        }
                    }
                    (*candidate_count)++;
                }
            }
        }
    }
    options.has_oasis_pair = 1;
    for (drop_index = 0;
         drop_index < GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT;
         drop_index++) {
        for (transition_index = 0;
             transition_index <
                 GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT;
             transition_index++) {
            options.response.oasis_drop =
                game_worldgen_aridity_response_oasis_drops[drop_index];
            options.response.transition_margin =
                game_worldgen_aridity_response_transition_margins[
                    transition_index];
            ok &= game_worldgen_aridity_projection_pair_id(
                &options, id, sizeof(id));
            if (*pair_count == 0) ok &= strcmp(id, "o00_t00") == 0;
            if (*pair_count == 35) ok &= strcmp(id, "o20_t25") == 0;
            (*pair_count)++;
        }
    }
    return ok && *candidate_count == 1920 && *pair_count == 36;
}

static int formula_case_ok(
    const GameWorldgenAridityResponseOptions *response,
    const GameWorldgenAridityCase *matrix_case) {
    WorldGenAridityResponseLimits limits;
    int below_50 = matrix_case->moisture < 50 ?
        50 - matrix_case->moisture : 0;
    int combined = clamp_int(
        response->arid_base +
            matrix_case->bias_desert * response->desert_bias_span / 100 +
            (matrix_case->moisture - 50) *
                response->moisture_compression_span / 25,
        0, 100);
    int band = clamp_int(
        12 - matrix_case->bias_desert * 10 / 100 + below_50 * 8 / 25,
        2, 20);
    int desert = clamp_int(combined - band, 0, 100);
    int oasis = 42 - matrix_case->drought * response->oasis_drop / 100;
    int transition = combined +
        matrix_case->drought * response->transition_margin / 100;
    return world_gen_aridity_response_calculate_expanded(
            matrix_case->moisture, matrix_case->drought,
            matrix_case->bias_desert, response->arid_base,
            response->desert_bias_span,
            response->moisture_compression_span, response->oasis_drop,
            response->transition_margin, &limits) &&
        limits.combined_arid_limit == combined &&
        limits.semi_arid_band == band && limits.desert_limit == desert &&
        limits.semi_arid_limit == combined && limits.oasis_limit == oasis &&
        limits.oasis_transition_limit == transition;
}

static int expanded_formula_grid_ok(int *case_count) {
    GameWorldgenAridityResponseOptions response;
    int base_index;
    int span_index;
    int divisor_index;
    int compression_index;
    int drop_index;
    int transition_index;
    int matrix_index;
    int ok = 1;
    memset(&response, 0, sizeof(response));
    *case_count = 0;
    for (base_index = 0; base_index < 5; base_index++) {
        for (span_index = 0; span_index < 6; span_index++) {
            for (divisor_index = 0; divisor_index < 8; divisor_index++) {
                for (compression_index = 0; compression_index < 8;
                     compression_index++) {
                    response.arid_base = BASES[base_index];
                    response.desert_bias_span = BIAS_SPANS[span_index];
                    response.drought_divisor = DIVISORS[divisor_index];
                    response.moisture_compression_span =
                        COMPRESSIONS[compression_index];
                    for (drop_index = 0;
                         drop_index < GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT;
                         drop_index++) {
                        for (transition_index = 0;
                             transition_index <
                                 GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT;
                             transition_index++) {
                            response.oasis_drop =
                                game_worldgen_aridity_response_oasis_drops[
                                    drop_index];
                            response.transition_margin =
                                game_worldgen_aridity_response_transition_margins[
                                    transition_index];
                            for (matrix_index = 0;
                                 matrix_index < GAME_WORLDGEN_ARIDITY_CASE_COUNT;
                                 matrix_index++) {
                                ok &= formula_case_ok(
                                    &response,
                                    &game_worldgen_aridity_cases[matrix_index]);
                                (*case_count)++;
                            }
                        }
                    }
                }
            }
        }
    }
    return ok && *case_count == 414720;
}

static int base_air_grid_ok(int *case_count) {
    static const int moistures[] = {0, 50, 100};
    static const int droughts[] = {0, 100};
    static const int noises[] = {-7, 0, 7};
    int divisor_index;
    int moisture_index;
    int drought_index;
    int noise_index;
    int ok = 1;
    *case_count = 0;
    for (divisor_index = 0; divisor_index < 8; divisor_index++) {
        for (moisture_index = 0; moisture_index < 3; moisture_index++) {
            for (drought_index = 0; drought_index < 2; drought_index++) {
                for (noise_index = 0; noise_index < 3; noise_index++) {
                    int actual = -1;
                    int expected = clamp_int(
                        18 + moistures[moisture_index] / 2 -
                            droughts[drought_index] /
                                DIVISORS[divisor_index] +
                            noises[noise_index] / 3,
                        4, 82);
                    ok &= world_gen_moisture_base_air_calculate(
                        moistures[moisture_index], droughts[drought_index],
                        DIVISORS[divisor_index], noises[noise_index],
                        &actual) && actual == expected;
                    (*case_count)++;
                }
            }
        }
    }
    return ok && *case_count == 144;
}

static int lifecycle_ok(int *case_count) {
    GameWorldgenAridityResponseDefaults defaults;
    GameWorldgenAridityProjectionOptions options;
    int divisor_index;
    int endpoint;
    int ok;
    *case_count = 0;
    memset(&options, 0, sizeof(options));
    ok = game_worldgen_aridity_projection_capture_defaults(&defaults) &&
        defaults.drought_divisor == 24 && defaults.desert_base == 10 &&
        defaults.desert_bias_span == 2 && defaults.semi_arid_width == 14 &&
        defaults.oasis_transition_margin == 15;
    for (divisor_index = 0; divisor_index < 8; divisor_index++) {
        ok &= game_worldgen_aridity_projection_enable_carrier(
            DIVISORS[divisor_index]) &&
            game_worldgen_aridity_projection_carrier_matches(
                DIVISORS[divisor_index]);
        game_worldgen_aridity_projection_reset_all();
        ok &= game_worldgen_aridity_projection_defaults_restored(&defaults);
        (*case_count)++;
    }
    options.has_candidate = 1;
    options.response.use_override = 1;
    for (endpoint = 0; endpoint < 2; endpoint++) {
        options.response.arid_base = BASES[endpoint ? 4 : 0];
        options.response.desert_bias_span = BIAS_SPANS[endpoint ? 5 : 0];
        options.response.drought_divisor = DIVISORS[endpoint ? 7 : 0];
        options.response.moisture_compression_span =
            COMPRESSIONS[endpoint ? 7 : 0];
        options.response.oasis_drop =
            game_worldgen_aridity_response_oasis_drops[
                endpoint ? GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT - 1 : 0];
        options.response.transition_margin =
            game_worldgen_aridity_response_transition_margins[
                endpoint ?
                    GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT - 1 : 0];
        ok &= game_worldgen_aridity_projection_enable_actual(&options) &&
            game_worldgen_aridity_projection_actual_matches(&options);
        game_worldgen_aridity_projection_reset_all();
        ok &= game_worldgen_aridity_projection_defaults_restored(&defaults);
        (*case_count)++;
    }
    game_worldgen_aridity_projection_reset_all();
    return ok && *case_count == 10 &&
        game_worldgen_aridity_projection_defaults_restored(&defaults);
}

static int run_formula_probe(void) {
    int ack_cases = 0;
    int candidate_count = 0;
    int pair_count = 0;
    int formula_cases = 0;
    int base_air_cases = 0;
    int lifecycle_cases = 0;
    uint64_t id_hash = 0;
    int ack_ok = acknowledgement_matrix_ok(&ack_cases);
    int grid_ok = candidate_and_pair_grid_ok(
        &candidate_count, &pair_count, &id_hash);
    int formula_ok = expanded_formula_grid_ok(&formula_cases);
    int base_air_ok = base_air_grid_ok(&base_air_cases);
    int state_ok = lifecycle_ok(&lifecycle_cases);
    int overall_ok = ack_ok && grid_ok && formula_ok && base_air_ok &&
        state_ok;
    printf("case=aridity_response_projection_formula ack_cases=%d "
           "ack_ok=%d candidate_cases=%d oasis_pair_cases=%d id_hash=%016llx "
           "grid_ok=%d formula_cases=%d formula_ok=%d base_air_cases=%d "
           "base_air_ok=%d lifecycle_cases=%d lifecycle_ok=%d "
           "worlds_generated=0 overall_ok=%d\n",
           ack_cases, ack_ok, candidate_count, pair_count,
           (unsigned long long)id_hash, grid_ok, formula_cases, formula_ok,
           base_air_cases, base_air_ok, lifecycle_cases, state_ok, overall_ok);
    return overall_ok ? 0 : 1;
}

int run_worldgen_aridity_response_projection_probe(void) {
    GameWorldgenAridityProjectionOptions options;
    int result;
    game_worldgen_aridity_projection_reset_all();
    if (!game_worldgen_aridity_projection_options_parse(&options)) {
        printf("case=aridity_response_projection_options parse_ok=0 "
               "worlds_generated=0 overall_ok=0\n");
        return 1;
    }
    if (options.action == GAME_WORLDGEN_ARIDITY_PROJECTION_ACTION_FORMULA) {
        result = run_formula_probe();
    } else {
        result = game_worldgen_aridity_response_projection_matrix_run(
            &options);
    }
    game_worldgen_aridity_projection_reset_all();
    return result;
}
