#include "game/game_worldgen_landform_semantics_probe.h"

#include "game/game_worldgen_lake_qualification_probe.h"

#include "core/world_types.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_context.h"

#include <stdint.h>
#include <string.h>

enum {
    CLASSIFY_W = 7,
    CLASSIFY_H = 5,
    END_TO_END_W = 96,
    END_TO_END_H = 64
};

typedef struct {
    const char *label;
    Geography expected;
    int relative;
    int slope;
    int curvature;
    int uplift;
    int ocean_distance;
    int moisture;
    Climate climate;
    uint16_t river_flags;
} RestorationCase;

typedef enum {
    OASIS_MOISTURE_AT_LOWER = 0,
    OASIS_MOISTURE_ABOVE_LOWER,
    OASIS_MOISTURE_BELOW_UPPER,
    OASIS_MOISTURE_AT_UPPER,
    OASIS_MOISTURE_MAXIMUM
} OasisMoisturePoint;

static int classify_fixture_create(WorldGenContext *context) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    int i;
    config.seed = UINT32_C(0x4c414e44);
    config.random_seed = 0;
    config.moisture = 50;
    config.bias_mountain = 0;
    if (!world_gen_context_create(context, &config, CLASSIFY_W, CLASSIFY_H,
                                  config.seed)) return 0;
    for (i = 0; i < context->tile_count; i++) {
        context->base_elevation[i] = 60;
        context->elevation[i] = 60;
        context->relative_altitude[i] = 10;
        context->slope[i] = 0;
        context->curvature[i] = 0;
        context->mountain_uplift[i] = 0;
        context->ocean_distance[i] = 5;
        context->moisture[i] = 20;
        context->temperature[i] = 50;
        context->precipitation[i] = 50;
        context->land_mask[i] = 1;
        context->geography[i] = GEO_PLAIN;
        context->climate[i] = CLIMATE_CONTINENTAL;
        context->ecology[i] = ECO_GRASSLAND;
        context->resource[i] = RESOURCE_FEATURE_NONE;
    }
    return 1;
}

static int count_selected_wetlands(const WorldGenContext *context,
                                   const int indices[3]) {
    int count = 0;
    int i;
    for (i = 0; i < 3; i++) count += context->geography[indices[i]] == GEO_WETLAND;
    return count;
}

static int rejected_lake_restoration_categories_contract(
    FILE *file, WorldGenContext *context) {
    static const RestorationCase cases[] = {
        {"plain", GEO_PLAIN, 10, 0, 0, 0, 5, 20, CLIMATE_CONTINENTAL, 0},
        {"basin", GEO_BASIN, 9, 0, -2, 0, 5, 20, CLIMATE_CONTINENTAL, 0},
        {"wetland", GEO_WETLAND, 10, 0, 0, 0, 5, 77, CLIMATE_CONTINENTAL, 0},
        {"hill", GEO_HILL, 18, 0, 0, 0, 5, 20, CLIMATE_CONTINENTAL, 0},
        {"plateau", GEO_PLATEAU, 29, 0, 0, 7, 5, 20, CLIMATE_CONTINENTAL, 0},
        {"oasis", GEO_OASIS, 10, 0, 0, 0, 5, -1, CLIMATE_DESERT,
         WORLD_GEN_RIVER_CHANNEL},
        {"coast", GEO_COAST, 10, 0, 0, 0, 1, 20, CLIMATE_CONTINENTAL, 0},
        {"delta", GEO_DELTA, 10, 0, 0, 0, 5, 20, CLIMATE_CONTINENTAL,
         WORLD_GEN_RIVER_DELTA}
    };
    int target = world_gen_context_index(context, 3, 2);
    int saved_moisture = context->config.moisture;
    int saved_drought = context->config.drought;
    int saved_bias_desert = context->config.bias_desert;
    WorldGenAridityResponseLimits oasis_limits = {0};
    int oasis_moisture = 0;
    int oasis_window_ok;
    int ok = 1;
    size_t i;
    context->config.bias_wetland = 0;
    context->config.moisture = 50;
    context->config.drought = 50;
    context->config.bias_desert = 50;
    oasis_window_ok = world_gen_aridity_response_current_limits(
        context->config.moisture, context->config.drought,
        context->config.bias_desert, &oasis_limits);
    if (oasis_window_ok) {
        oasis_moisture = oasis_limits.oasis_limit + 1;
        oasis_window_ok = oasis_moisture < oasis_limits.oasis_transition_limit;
    }
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const RestorationCase *row = &cases[i];
        int row_moisture = row->expected == GEO_OASIS
            ? oasis_moisture : row->moisture;
        int input_ok = row->expected != GEO_OASIS || oasis_window_ok;
        Geography underlying;
        int lake_ok;
        int restored_ok;
        context->relative_altitude[target] = (int16_t)row->relative;
        context->slope[target] = (int16_t)row->slope;
        context->curvature[target] = (int16_t)row->curvature;
        context->mountain_uplift[target] = (int16_t)row->uplift;
        context->ocean_distance[target] = (int16_t)row->ocean_distance;
        context->moisture[target] = (int16_t)row_moisture;
        context->climate[target] = (uint8_t)row->climate;
        context->river_flags[target] =
            (uint16_t)(row->river_flags | WORLD_GEN_RIVER_LAKE);
        if (row->expected == GEO_OASIS) {
            input_ok &= world_gen_classify_response_oasis_predicate_for_pair(
                context, target, row->climate,
                world_gen_aridity_response_oasis_drop(),
                world_gen_aridity_response_transition_margin());
        }
        underlying = world_gen_classify_underlying_land(
            context, target, target % context->width, target / context->width,
            row->climate);
        lake_ok = input_ok && underlying == row->expected &&
                  world_gen_classify_final(context) &&
                  context->geography[target] == GEO_LAKE;
        context->river_flags[target] = row->river_flags;
        context->climate[target] = (uint8_t)row->climate;
        restored_ok = world_gen_classify_final(context) &&
            context->geography[target] == row->expected &&
            world_gen_classify_underlying_land(
                context, target, target % context->width,
                target / context->width,
                (Climate)context->climate[target]) == row->expected;
        fprintf(file,
                "case=landform_rejected_lake_restore category=%s expected=%d "
                "moisture=%d lower=%d upper=%d input_ok=%d underlying=%d "
                "lake_override=%d restored=%d ok=%d\n",
                row->label, row->expected, row_moisture,
                row->expected == GEO_OASIS ? oasis_limits.oasis_limit : -1,
                row->expected == GEO_OASIS
                    ? oasis_limits.oasis_transition_limit : -1,
                input_ok, underlying, lake_ok, restored_ok,
                lake_ok && restored_ok);
        ok &= lake_ok && restored_ok;
    }
    context->config.moisture = saved_moisture;
    context->config.drought = saved_drought;
    context->config.bias_desert = saved_bias_desert;
    return ok;
}

static int legacy_oasis_helper_contract(FILE *file) {
    int limit_0 = world_gen_oasis_moisture_limit(0);
    int limit_50 = world_gen_oasis_moisture_limit(50);
    int limit_100 = world_gen_oasis_moisture_limit(100);
    int ok = limit_0 == 42 && limit_50 == 33 && limit_100 == 24;
    fprintf(file,
            "case=landform_legacy_oasis_helper role=legacy_only "
            "limits=%d/%d/%d expected=42/33/24 ok=%d\n",
            limit_0, limit_50, limit_100, ok);
    return ok;
}

static int oasis_moisture_for_point(
    const WorldGenAridityResponseLimits *limits, OasisMoisturePoint point) {
    if (!limits) return -1;
    switch (point) {
        case OASIS_MOISTURE_AT_LOWER:
            return limits->oasis_limit;
        case OASIS_MOISTURE_ABOVE_LOWER:
            return limits->oasis_limit + 1;
        case OASIS_MOISTURE_BELOW_UPPER:
            return limits->oasis_transition_limit - 1;
        case OASIS_MOISTURE_AT_UPPER:
            return limits->oasis_transition_limit;
        case OASIS_MOISTURE_MAXIMUM:
            return 100;
    }
    return -1;
}

static int current_oasis_fixture_case(
    FILE *file, WorldGenContext *context, int target, const char *label,
    int drought, Climate climate, int temperature, int river_channel,
    OasisMoisturePoint point, int expected_oasis) {
    WorldGenAridityResponseLimits limits = {0};
    int limits_ok;
    int moisture;
    int shared_expected = 0;
    int classify_ok;
    int actual_oasis;
    int ok;
    context->config.drought = drought;
    limits_ok = world_gen_aridity_response_current_limits(
        context->config.moisture, context->config.drought,
        context->config.bias_desert, &limits);
    moisture = limits_ok ? oasis_moisture_for_point(&limits, point) : -1;
    context->climate[target] = (uint8_t)climate;
    context->temperature[target] = (int16_t)temperature;
    context->moisture[target] = (int16_t)moisture;
    context->river_flags[target] = river_channel
        ? WORLD_GEN_RIVER_CHANNEL : 0;
    if (limits_ok) {
        shared_expected = world_gen_classify_response_visible_oasis_for_pair(
            context, target, climate,
            world_gen_aridity_response_oasis_drop(),
            world_gen_aridity_response_transition_margin());
    }
    classify_ok = limits_ok && world_gen_classify_final(context);
    actual_oasis = classify_ok && context->geography[target] == GEO_OASIS;
    ok = classify_ok && shared_expected == expected_oasis &&
        actual_oasis == expected_oasis &&
        (!expected_oasis || context->ecology[target] == ECO_GRASSLAND);
    fprintf(file,
            "case=landform_current_oasis label=%s drought=%d climate=%d "
            "temperature=%d river=%d moisture=%d lower=%d upper=%d "
            "shared=%d actual=%d expected=%d ok=%d\n",
            label, drought, climate, temperature, river_channel, moisture,
            limits.oasis_limit, limits.oasis_transition_limit,
            shared_expected, actual_oasis, expected_oasis, ok);
    return ok;
}

static int classification_fixture_contracts(FILE *file) {
    static const int biases[3] = {0, 50, 100};
    static const int expected[3] = {1, 2, 3};
    static const int moisture[3] = {77, 71, 65};
    WorldGenContext context;
    int wet_indices[3];
    int wet_counts[3] = {0, 0, 0};
    int wet_ok = 1;
    int restoration_ok;
    int legacy_oasis_ok;
    int current_oasis_ok;
    int target;
    int i;
    memset(&context, 0, sizeof(context));
    if (!classify_fixture_create(&context)) {
        fprintf(file, "case=landform_classify_fixture created=0 ok=0\n");
        return 0;
    }
    for (i = 0; i < 3; i++) {
        wet_indices[i] = world_gen_context_index(&context, 2 + i, 2);
        context.moisture[wet_indices[i]] = (int16_t)moisture[i];
    }
    for (i = 0; i < 3; i++) {
        context.config.bias_wetland = biases[i];
        wet_ok &= world_gen_classify_final(&context);
        wet_counts[i] = count_selected_wetlands(&context, wet_indices);
        wet_ok &= wet_counts[i] == expected[i];
    }
    target = wet_indices[0];
    context.config.bias_wetland = 100;
    context.river_flags[target] = WORLD_GEN_RIVER_LAKE;
    restoration_ok = world_gen_classify_final(&context) &&
        context.geography[target] == GEO_LAKE &&
        context.ecology[target] == ECO_NONE &&
        context.resource[target] == RESOURCE_FEATURE_FISHERY;
    context.river_flags[target] = 0;
    restoration_ok &= world_gen_classify_final(&context) &&
        context.geography[target] == GEO_WETLAND &&
        world_gen_classify_underlying_land(
            &context, target, target % context.width, target / context.width,
            (Climate)context.climate[target]) == GEO_WETLAND;
    target = world_gen_context_index(&context, 3, 3);
    context.config.bias_wetland = 50;
    legacy_oasis_ok = legacy_oasis_helper_contract(file);
    current_oasis_ok = current_oasis_fixture_case(
        file, &context, target, "d0_macro_lower_rejected", 0,
        CLIMATE_DESERT, 20, 1, OASIS_MOISTURE_AT_LOWER, 0);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d0_macro_above_lower", 0,
        CLIMATE_DESERT, 20, 1, OASIS_MOISTURE_ABOVE_LOWER, 1);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d0_macro_no_upper", 0,
        CLIMATE_DESERT, 20, 1, OASIS_MOISTURE_MAXIMUM, 1);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d100_macro_lower_rejected", 100,
        CLIMATE_DESERT, 20, 1, OASIS_MOISTURE_AT_LOWER, 0);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d100_macro_above_lower", 100,
        CLIMATE_DESERT, 20, 1, OASIS_MOISTURE_ABOVE_LOWER, 1);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d100_macro_below_upper", 100,
        CLIMATE_DESERT, 20, 1, OASIS_MOISTURE_BELOW_UPPER, 1);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d100_macro_upper_rejected", 100,
        CLIMATE_DESERT, 20, 1, OASIS_MOISTURE_AT_UPPER, 0);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d100_hot_transition", 100,
        CLIMATE_CONTINENTAL, 29, 1, OASIS_MOISTURE_ABOVE_LOWER, 1);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d0_transition_rejected", 0,
        CLIMATE_CONTINENTAL, 29, 1, OASIS_MOISTURE_ABOVE_LOWER, 0);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d100_cool_transition_rejected", 100,
        CLIMATE_CONTINENTAL, 28, 1, OASIS_MOISTURE_ABOVE_LOWER, 0);
    current_oasis_ok &= current_oasis_fixture_case(
        file, &context, target, "d100_no_river_rejected", 100,
        CLIMATE_DESERT, 20, 0, OASIS_MOISTURE_ABOVE_LOWER, 0);
    context.config.drought = 50;
    restoration_ok &=
        rejected_lake_restoration_categories_contract(file, &context);
    fprintf(file,
            "case=landform_classify_fixture wet_bias_0=%d wet_bias_50=%d "
            "wet_bias_100=%d wet_expected=1/2/3 nondecreasing=%d "
            "lake_restoration=%d legacy_oasis_helper=%d "
            "current_oasis_contract=%d ok=%d\n",
            wet_counts[0], wet_counts[1], wet_counts[2],
            wet_counts[0] <= wet_counts[1] && wet_counts[1] <= wet_counts[2],
            restoration_ok, legacy_oasis_ok, current_oasis_ok,
            wet_ok && restoration_ok && legacy_oasis_ok && current_oasis_ok);
    world_gen_context_destroy(&context);
    return wet_ok && restoration_ok && legacy_oasis_ok && current_oasis_ok;
}

static void count_landforms(const WorldGenContext *context, int *wetlands,
                            int *lakes, int *oases) {
    int i;
    *wetlands = 0;
    *lakes = 0;
    *oases = 0;
    for (i = 0; i < context->tile_count; i++) {
        *wetlands += context->geography[i] == GEO_WETLAND;
        *lakes += context->geography[i] == GEO_LAKE;
        *oases += context->geography[i] == GEO_OASIS;
    }
}

static int end_to_end_wetland_contract(FILE *file,
                                       const WorldGenConfig *base_config) {
    static const int biases[3] = {0, 50, 100};
    WorldGenConfig baseline = base_config ? *base_config : DEFAULT_WORLD_GEN_CONFIG;
    uint32_t seeds[3];
    int totals[3] = {0, 0, 0};
    int prepare_errors = 0;
    int seed_index;
    int bias_index;
    seeds[0] = baseline.seed ? baseline.seed : UINT32_C(3300337);
    seeds[1] = seeds[0] ^ UINT32_C(0x9e3779b9);
    seeds[2] = seeds[0] ^ UINT32_C(0x3c6ef372);
    for (seed_index = 0; seed_index < 3; seed_index++) {
        for (bias_index = 0; bias_index < 3; bias_index++) {
            WorldGenConfig config = baseline;
            WorldGenContext *context;
            int wetlands = 0;
            int lakes = 0;
            int oases = 0;
            int prepared;
            config.seed = seeds[seed_index];
            config.random_seed = 0;
            config.bias_wetland = biases[bias_index];
            context = world_gen_prepare_for_dimensions(
                &config, END_TO_END_W, END_TO_END_H);
            prepared = context != NULL;
            if (context) {
                count_landforms(context, &wetlands, &lakes, &oases);
                totals[bias_index] += wetlands;
                world_gen_release_prepared(context);
            } else {
                prepare_errors++;
            }
            fprintf(file,
                    "case=landform_wetland_seed seed=%u bias=%d map=%dx%d "
                    "wetlands=%d lakes=%d oases=%d prepared=%d\n",
                    seeds[seed_index], biases[bias_index], END_TO_END_W,
                    END_TO_END_H, wetlands, lakes, oases, prepared);
        }
    }
    {
        int nondecreasing = totals[0] <= totals[1] && totals[1] <= totals[2];
        int strict = totals[0] < totals[1] && totals[1] < totals[2];
        int ok = prepare_errors == 0 && nondecreasing && strict;
        fprintf(file,
                "case=landform_wetland_aggregate seeds=3 map=%dx%d bias_0=%d "
                "bias_50=%d bias_100=%d nondecreasing=%d strict=%d "
                "prepare_errors=%d ok=%d\n",
                END_TO_END_W, END_TO_END_H, totals[0], totals[1], totals[2],
                nondecreasing, strict, prepare_errors, ok);
        return ok;
    }
}

int game_worldgen_landform_semantics_probe_run(
    FILE *file, const WorldGenConfig *base_config) {
    int ok;
    if (!file) return 0;
    ok = game_worldgen_lake_qualification_probe_run(file);
    ok &= classification_fixture_contracts(file);
    ok &= end_to_end_wetland_contract(file, base_config);
    fprintf(file, "case=landform_semantics_summary ok=%d\n", ok);
    return ok;
}
