#include "game/game_worldgen_aridity_probe.h"

#include "game/game_worldgen_aridity_calibration_matrix.h"
#include "game/game_worldgen_aridity_calibration_metrics.h"
#include "game/game_worldgen_aridity_calibration_options.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_moisture.h"

#include <stdint.h>
#include <string.h>

static const uint32_t MATRIX_SEEDS[] = {2026072301u, 2026072302u};

int game_worldgen_aridity_formula_probe_run(FILE *file) {
    static const char *const ack_values[] = {
        NULL,
        "",
        "ver037a_aridity_recalibration",
        "VER037A_ARIDITY_RECALIBRATIO",
        "VER037A_ARIDITY_RECALIBRATION_SUFFIX",
        "arbitrary-nonempty",
        "VER037A_ARIDITY_RECALIBRATION"
    };
    static const int ack_expected[] = {0, 0, 0, 0, 0, 0, 1};
    WorldGenAridityResponseLimits response_0;
    WorldGenAridityResponseLimits response_50;
    WorldGenAridityResponseLimits response_100;
    int legacy_desert_base = world_gen_desert_base();
    int legacy_desert_span = world_gen_desert_bias_span();
    int legacy_semi_arid_width = world_gen_semi_arid_width();
    int legacy_transition_margin = world_gen_oasis_transition_margin();
    int legacy_desert_0 = legacy_desert_base;
    int legacy_desert_50 = legacy_desert_base + 50 * legacy_desert_span / 100;
    int legacy_desert_100 = legacy_desert_base + legacy_desert_span;
    int drought_divisor = world_gen_moisture_drought_divisor();
    int ack_ok = 1;
    int lifecycle_ok;
    int ok;
    size_t ack_index;
    int legacy_desert_ok = world_gen_desert_moisture_limit(0) == legacy_desert_0 &&
        world_gen_desert_moisture_limit(50) == legacy_desert_50 &&
        world_gen_desert_moisture_limit(100) == legacy_desert_100;
    int legacy_semi_arid_ok =
        world_gen_semi_arid_moisture_limit(0) ==
            legacy_desert_0 + legacy_semi_arid_width &&
        world_gen_semi_arid_moisture_limit(50) ==
            legacy_desert_50 + legacy_semi_arid_width &&
        world_gen_semi_arid_moisture_limit(100) ==
            legacy_desert_100 + legacy_semi_arid_width;
    int legacy_oasis_ok = world_gen_oasis_moisture_limit(0) == 42 &&
        world_gen_oasis_moisture_limit(50) == 33 &&
        world_gen_oasis_moisture_limit(100) == 24;
    int legacy_transition_ok =
        world_gen_oasis_transition_moisture_limit(0, 0) ==
            legacy_desert_0 + legacy_semi_arid_width &&
        world_gen_oasis_transition_moisture_limit(50, 50) ==
            legacy_desert_50 + legacy_semi_arid_width +
                50 * legacy_transition_margin / 100 &&
        world_gen_oasis_transition_moisture_limit(100, 100) ==
            legacy_desert_100 + legacy_semi_arid_width +
                legacy_transition_margin;
    int production_coefficients_ok = drought_divisor == 24 &&
        world_gen_aridity_response_arid_base() == 31 &&
        world_gen_aridity_response_desert_bias_span() == 4 &&
        world_gen_aridity_response_drought_classification_span() == 2 &&
        world_gen_aridity_response_moisture_compression_span() == 12 &&
        world_gen_aridity_response_oasis_drop() == 20 &&
        world_gen_aridity_response_transition_margin() == 2;
    int production_limits_ok =
        world_gen_aridity_response_current_limits(50, 0, 0, &response_0) &&
        world_gen_aridity_response_current_limits(50, 50, 50, &response_50) &&
        world_gen_aridity_response_current_limits(50, 100, 100, &response_100) &&
        response_0.oasis_limit == 42 &&
        response_50.oasis_limit == 32 && response_100.oasis_limit == 22 &&
        response_0.oasis_transition_limit == 31 &&
        response_50.oasis_transition_limit == 34 &&
        response_100.oasis_transition_limit == 37;
    int state_ok = drought_divisor > 0 && legacy_desert_span > 0 &&
        legacy_desert_base > 0 && legacy_semi_arid_width > 0 &&
        legacy_transition_margin >= 0 &&
        !world_gen_moisture_validation_drought_divisor_active() &&
        !world_gen_classify_validation_aridity_active() &&
        !world_gen_aridity_response_validation_active();
    for (ack_index = 0;
         ack_index < sizeof(ack_values) / sizeof(ack_values[0]); ack_index++) {
        ack_ok &= game_worldgen_aridity_calibration_acknowledgement_matches(
            ack_values[ack_index]) == ack_expected[ack_index];
    }
    lifecycle_ok = state_ok;
    ok = legacy_desert_ok && legacy_semi_arid_ok && legacy_oasis_ok &&
        legacy_transition_ok && production_coefficients_ok &&
        production_limits_ok && state_ok && ack_ok && lifecycle_ok;
    if (file) {
        fprintf(file,
                "case=aridity_formula drought_divisor=%d legacy_desert_base=%d "
                "legacy_desert_span=%d legacy_semi_arid_width=%d "
                "legacy_transition_margin=%d legacy_desert=%d/%d/%d "
                "legacy_semi_arid=%d/%d/%d legacy_oasis_lower=42/33/24 "
                "legacy_desert_ok=%d legacy_semi_arid_ok=%d "
                "legacy_oasis_ok=%d legacy_transition_ok=%d "
                "production_coefficients=31/4/2/12/20/2 "
                "production_oasis_lower=42/32/22 "
                "production_oasis_upper=31/34/37 "
                "production_coefficients_ok=%d production_limits_ok=%d "
                "acknowledgement_cases=7 ack_ok=%d lifecycle_ok=%d "
                "state_ok=%d ok=%d\n",
                drought_divisor, legacy_desert_base, legacy_desert_span,
                legacy_semi_arid_width, legacy_transition_margin,
                legacy_desert_0, legacy_desert_50, legacy_desert_100,
                legacy_desert_0 + legacy_semi_arid_width,
                legacy_desert_50 + legacy_semi_arid_width,
                legacy_desert_100 + legacy_semi_arid_width,
                legacy_desert_ok, legacy_semi_arid_ok, legacy_oasis_ok,
                legacy_transition_ok, production_coefficients_ok,
                production_limits_ok, ack_ok, lifecycle_ok, state_ok, ok);
    }
    return ok;
}

int run_worldgen_aridity_formula_probe(void) {
    GameWorldgenAridityCalibrationDefaults defaults;
    GameWorldgenAridityCalibrationOptions options;
    int requested = game_worldgen_aridity_calibration_requested();
    int active_before =
        world_gen_moisture_validation_drought_divisor_active() ||
        world_gen_classify_validation_aridity_active() ||
        world_gen_aridity_response_validation_active();
    int formula_ok = game_worldgen_aridity_formula_probe_run(NULL);
    int parse_ok = !requested;
    int activated = 0;
    int lifecycle_ok = game_worldgen_aridity_calibration_capture_defaults(
        &defaults);
    if (requested) {
        parse_ok = game_worldgen_aridity_calibration_options_parse(&options);
        if (parse_ok && options.use_override) {
            activated = game_worldgen_aridity_calibration_enable_override(
                &options) &&
                game_worldgen_aridity_calibration_override_matches(&options);
            lifecycle_ok &= activated;
        } else {
            lifecycle_ok = 0;
        }
    }
    game_worldgen_aridity_calibration_reset_overrides();
    lifecycle_ok &= game_worldgen_aridity_calibration_defaults_restored(
        &defaults);
    {
        int active_after =
            world_gen_moisture_validation_drought_divisor_active() ||
            world_gen_classify_validation_aridity_active() ||
            world_gen_aridity_response_validation_active();
        int ok = !active_before && formula_ok && parse_ok && lifecycle_ok &&
            !active_after && (requested == activated);
        printf("case=aridity_formula_environment requested=%d parse_ok=%d "
               "override_activated=%d active_before=%d active_after=%d "
               "formula_ok=%d lifecycle_ok=%d ok=%d\n",
               requested, parse_ok, activated, active_before, active_after,
               formula_ok, lifecycle_ok, ok);
        return ok ? 0 : 1;
    }
}

int run_worldgen_aridity_smoke_probe(void) {
    char path[1024];
    GameWorldgenAridityResult result;
    const GameWorldgenAridityCase smoke_case = {50, 0, 0};
    FILE *file;
    int formula_ok;
    int row_ok;
    int io_ok;
    if (!game_worldgen_aridity_output_path(
            path, sizeof(path), "aridity_smoke.txt") ||
        !game_worldgen_aridity_output_path_available(path)) return 2;
    file = fopen(path, "wb");
    if (!file) return 2;
    formula_ok = game_worldgen_aridity_formula_probe_run(file);
    row_ok = game_worldgen_aridity_run_world(
        MATRIX_SEEDS[0], 0, &smoke_case, &result);
    fprintf(file,
            "case=aridity_smoke seed=%u map=%s dimensions=%dx%d "
            "moisture=%d drought=%d bias_desert=%d physical_hash=%016llx "
            "land=%d desert=%d semi_arid=%d non_arid=%d oasis=%d "
            "arid_channels=%d predicate=%d reachable=%d suppressed=%d "
            "wetland=%d semantic_errors=%d generated=%d ok=%d\n",
            MATRIX_SEEDS[0], game_worldgen_aridity_map_names[0],
            game_worldgen_aridity_map_widths[0],
            game_worldgen_aridity_map_heights[0], smoke_case.moisture,
            smoke_case.drought, smoke_case.bias_desert,
            (unsigned long long)result.physical_hash, result.land_count,
            result.desert_count, result.semi_arid_count,
            result.non_arid_count, result.oasis_count,
            result.arid_channel_count, result.oasis_predicate_count,
            result.oasis_reachable_count, result.oasis_suppressed_count,
            result.wetland_count, result.oasis_semantic_errors,
            result.generated, formula_ok && row_ok);
    io_ok = fflush(file) == 0;
    if (fclose(file) != 0) io_ok = 0;
    if (!io_ok) return 2;
    printf("worldgen aridity smoke summary: %s\n", path);
    return formula_ok && row_ok ? 0 : 1;
}

static int run_legacy_matrix(void) {
    char csv_path[1024];
    char summary_path[1024];
    FILE *csv;
    FILE *summary;
    uint64_t matrix_hash = UINT64_C(1469598103934665603);
    int row_index = 0;
    int generation_failures = 0;
    int non_arid_failures = 0;
    int semantic_failures = 0;
    int hash_failures = 0;
    int formula_ok;
    int io_ok = 1;
    size_t seed_index;
    int map_size;
    int case_index;
    if (!game_worldgen_aridity_output_path(
            csv_path, sizeof(csv_path), "aridity_matrix.csv") ||
        !game_worldgen_aridity_output_path(
            summary_path, sizeof(summary_path), "aridity_matrix_summary.txt") ||
        !game_worldgen_aridity_output_path_available(csv_path) ||
        !game_worldgen_aridity_output_path_available(summary_path)) return 2;
    csv = fopen(csv_path, "wb");
    summary = fopen(summary_path, "wb");
    if (!csv || !summary) {
        if (csv) fclose(csv);
        if (summary) fclose(summary);
        return 2;
    }
    io_ok &= game_worldgen_aridity_write_csv_header(csv);
    formula_ok = game_worldgen_aridity_formula_probe_run(summary);
    for (seed_index = 0; seed_index < 2; seed_index++) {
        for (map_size = 0; map_size < GAME_WORLDGEN_ARIDITY_MAP_COUNT;
             map_size++) {
            for (case_index = 0;
                 case_index < GAME_WORLDGEN_ARIDITY_CASE_COUNT; case_index++) {
                GameWorldgenAridityResult result;
                const GameWorldgenAridityCase *matrix_case =
                    &game_worldgen_aridity_cases[case_index];
                int row_ok = game_worldgen_aridity_run_world(
                    MATRIX_SEEDS[seed_index], map_size, matrix_case, &result);
                generation_failures += !result.generated;
                non_arid_failures +=
                    result.generated && result.non_arid_count <= 0;
                semantic_failures += result.oasis_semantic_errors != 0 ||
                    result.oasis_count != result.oasis_reachable_count;
                hash_failures += result.generated && result.physical_hash == 0;
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, MATRIX_SEEDS[seed_index]);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, (uint32_t)map_size);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, (uint32_t)matrix_case->moisture);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, (uint32_t)matrix_case->drought);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, (uint32_t)matrix_case->bias_desert);
                matrix_hash = game_worldgen_aridity_hash_mix(
                    matrix_hash, result.physical_hash);
                io_ok &= game_worldgen_aridity_write_csv_row(
                    csv, row_index, MATRIX_SEEDS[seed_index], map_size,
                    matrix_case, &result);
                row_index++;
                if (!row_ok) semantic_failures +=
                    result.generated && result.non_arid_count > 0 &&
                    result.physical_hash != 0 &&
                    result.oasis_semantic_errors == 0;
            }
        }
    }
    {
        int ok = formula_ok && row_index == 48 && generation_failures == 0 &&
            non_arid_failures == 0 && semantic_failures == 0 &&
            hash_failures == 0 && io_ok;
        io_ok &= fprintf(summary,
            "case=aridity_matrix seeds=2 map_sizes=4 configurations=6 "
            "worlds=%d generation_failures=%d non_arid_failures=%d "
            "semantic_failures=%d hash_failures=%d matrix_hash=%016llx "
            "ok=%d\n",
            row_index, generation_failures, non_arid_failures,
            semantic_failures, hash_failures,
            (unsigned long long)matrix_hash, ok) > 0;
        if (fflush(csv) != 0) io_ok = 0;
        if (fflush(summary) != 0) io_ok = 0;
        if (fclose(csv) != 0) io_ok = 0;
        if (fclose(summary) != 0) io_ok = 0;
        if (!io_ok) return 2;
        printf("worldgen aridity matrix: %s\n", csv_path);
        printf("worldgen aridity matrix summary: %s\n", summary_path);
        return ok ? 0 : 1;
    }
}

int run_worldgen_aridity_matrix_probe(void) {
    if (game_worldgen_aridity_calibration_requested()) {
        return game_worldgen_aridity_calibration_matrix_run();
    }
    return run_legacy_matrix();
}
