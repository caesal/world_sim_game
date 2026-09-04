#include "game/game_worldgen_aridity_response_probe.h"

#include "game/game_worldgen_aridity_response_matrix.h"
#include "game/game_worldgen_aridity_response_metrics.h"
#include "game/game_worldgen_aridity_response_options.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_moisture.h"

#include <stdio.h>
#include <string.h>

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int formula_case_ok(
    int moisture, int drought, int bias_desert, int compression_span,
    int oasis_drop, int transition_margin) {
    WorldGenAridityResponseLimits limits;
    int combined = clamp_int(
        30 + bias_desert * 2 / 100 +
            (moisture - 50) * compression_span / 25,
        0, 100);
    int band = clamp_int(
        12 - bias_desert * 10 / 100 +
            (moisture < 50 ? 50 - moisture : 0) * 8 / 25,
        2, 20);
    int desert = clamp_int(combined - band, 0, 100);
    int oasis = 42 - drought * oasis_drop / 100;
    int transition = combined + drought * transition_margin / 100;
    return world_gen_aridity_response_calculate(
            moisture, drought, bias_desert, compression_span, oasis_drop,
            transition_margin, &limits) &&
        limits.combined_arid_limit == combined &&
        limits.semi_arid_band == band && limits.desert_limit == desert &&
        limits.semi_arid_limit == combined && limits.oasis_limit == oasis &&
        limits.oasis_transition_limit == transition;
}

static int acknowledgement_matrix_ok(void) {
    static const char *const values[] = {
        NULL,
        "",
        "ver037a_aridity_response_pilot",
        "VER037A_ARIDITY_RESPONSE_PILOT_extra",
        "prefix_VER037A_ARIDITY_RESPONSE_PILOT",
        "VER037A_ARIDITY_RECALIBRATION",
        "VER037A_ARIDITY_RESPONSE_PILOT"
    };
    static const int expected[] = {0, 0, 0, 0, 0, 0, 1};
    int index;
    int ok = 1;
    for (index = 0; index < 7; index++) {
        ok &= game_worldgen_aridity_response_acknowledgement_matches(
            values[index]) == expected[index];
    }
    return ok;
}

static int base_air_formula_ok(int *case_count) {
    static const struct {
        int world_moisture;
        int drought;
        int drought_divisor;
        int noise;
        int expected;
    } cases[] = {
        {50, 0, 20, 0, 43},
        {50, 100, 20, 0, 38},
        {50, 100, 24, 0, 39},
        {50, 100, 28, 0, 40},
        {49, 100, 24, -1, 38},
        {49, 100, 24, -4, 37},
        {0, 100, 20, -50, 4},
        {100, 0, 20, 50, 82}
    };
    int index;
    int ok = 1;
    if (case_count) *case_count = 0;
    for (index = 0; index < (int)(sizeof(cases) / sizeof(cases[0])); index++) {
        int actual = -1;
        ok &= world_gen_moisture_base_air_calculate(
            cases[index].world_moisture, cases[index].drought,
            cases[index].drought_divisor, cases[index].noise, &actual) &&
            actual == cases[index].expected;
        if (case_count) (*case_count)++;
    }
    {
        int ignored;
        ok &= !world_gen_moisture_base_air_calculate(
            50, 100, 0, 0, &ignored);
        ok &= !world_gen_moisture_base_air_calculate(
            50, 100, 24, 0, NULL);
        if (case_count) *case_count += 2;
    }
    return ok;
}

static int formula_and_lifecycle_probe(void) {
    static const int drought_values[] = {20, 24, 28};
    static const int compression_values[] = {6, 8, 10};
    static const int moisture_values[] = {0, 24, 25, 49, 50, 51, 75, 100};
    static const int drought_inputs[] = {0, 100};
    static const int bias_values[] = {0, 100};
    GameWorldgenAridityResponseDefaults defaults = {0};
    GameWorldgenAridityResponseOptions sample;
    int formula_cases = 0;
    int base_air_cases = 0;
    int lifecycle_cases = 0;
    int base_air_ok = base_air_formula_ok(&base_air_cases);
    int formula_ok = 1;
    int lifecycle_ok;
    int ack_ok = acknowledgement_matrix_ok();
    int di;
    int ci;
    int mi;
    int dri;
    int bi;
    int oi;
    int ti;
    game_worldgen_aridity_response_reset_overrides();
    lifecycle_ok = game_worldgen_aridity_response_capture_defaults(&defaults) &&
        defaults.drought_divisor == 24 && defaults.desert_base == 10 &&
        defaults.desert_bias_span == 2 && defaults.semi_arid_width == 14 &&
        defaults.oasis_transition_margin == 15;
    for (ci = 0; ci < 3; ci++) {
        for (mi = 0; mi < 8; mi++) {
            for (dri = 0; dri < 2; dri++) {
                for (bi = 0; bi < 2; bi++) {
                    for (oi = 0;
                         oi < GAME_WORLDGEN_ARIDITY_RESPONSE_DROP_COUNT;
                         oi++) {
                        for (ti = 0;
                             ti <
                                 GAME_WORLDGEN_ARIDITY_RESPONSE_TRANSITION_COUNT;
                             ti++) {
                            formula_ok &= formula_case_ok(
                                moisture_values[mi], drought_inputs[dri],
                                bias_values[bi], compression_values[ci],
                                game_worldgen_aridity_response_oasis_drops[oi],
                                game_worldgen_aridity_response_transition_margins[
                                    ti]);
                            formula_cases++;
                        }
                    }
                }
            }
        }
    }
    memset(&sample, 0, sizeof(sample));
    sample.action = GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_SCREEN;
    sample.action_name = "screen";
    sample.use_override = 1;
    sample.arid_base = 30;
    sample.desert_bias_span = 2;
    sample.oasis_drop = 8;
    sample.transition_margin = 0;
    for (di = 0; di < 3; di++) {
        for (ci = 0; ci < 3; ci++) {
            sample.drought_divisor = drought_values[di];
            sample.moisture_compression_span = compression_values[ci];
            lifecycle_ok &=
                game_worldgen_aridity_response_defaults_restored(&defaults) &&
                game_worldgen_aridity_response_enable_override(&sample) &&
                game_worldgen_aridity_response_override_matches(&sample);
            game_worldgen_aridity_response_reset_overrides();
            lifecycle_ok &=
                game_worldgen_aridity_response_defaults_restored(&defaults);
            lifecycle_cases++;
        }
    }
    game_worldgen_aridity_response_reset_overrides();
    {
        int overall_ok = ack_ok && base_air_ok && formula_ok && lifecycle_ok &&
            game_worldgen_aridity_response_defaults_restored(&defaults);
        printf("case=aridity_response_formula acknowledgement_cases=7 "
               "ack_ok=%d base_air_cases=%d base_air_ok=%d "
               "formula_cases=%d formula_ok=%d "
               "lifecycle_cases=%d lifecycle_ok=%d worlds_generated=0 "
               "overall_ok=%d\n",
               ack_ok, base_air_cases, base_air_ok, formula_cases, formula_ok,
               lifecycle_cases, lifecycle_ok, overall_ok);
        return overall_ok ? 0 : 1;
    }
}

int run_worldgen_aridity_response_pilot_probe(void) {
    GameWorldgenAridityResponseOptions options;
    int result;
    game_worldgen_aridity_response_reset_overrides();
    if (!game_worldgen_aridity_response_options_parse(&options)) {
        printf("case=aridity_response_options parse_ok=0 worlds_generated=0 "
               "overall_ok=0\n");
        return 1;
    }
    if (options.action == GAME_WORLDGEN_ARIDITY_RESPONSE_ACTION_FORMULA) {
        result = formula_and_lifecycle_probe();
    } else {
        result = game_worldgen_aridity_response_matrix_run(&options);
    }
    game_worldgen_aridity_response_reset_overrides();
    return result;
}
