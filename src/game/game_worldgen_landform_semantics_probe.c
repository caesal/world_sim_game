#include "game/game_worldgen_landform_semantics_probe.h"

#include "game/game_worldgen_lake_qualification_probe.h"

#include "core/world_types.h"
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
        {"oasis", GEO_OASIS, 10, 0, 0, 0, 5, 43, CLIMATE_DESERT,
         WORLD_GEN_RIVER_CHANNEL},
        {"coast", GEO_COAST, 10, 0, 0, 0, 1, 20, CLIMATE_CONTINENTAL, 0},
        {"delta", GEO_DELTA, 10, 0, 0, 0, 5, 20, CLIMATE_CONTINENTAL,
         WORLD_GEN_RIVER_DELTA}
    };
    int target = world_gen_context_index(context, 3, 2);
    int ok = 1;
    size_t i;
    context->config.bias_wetland = 0;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const RestorationCase *row = &cases[i];
        Geography underlying;
        int lake_ok;
        int restored_ok;
        context->relative_altitude[target] = (int16_t)row->relative;
        context->slope[target] = (int16_t)row->slope;
        context->curvature[target] = (int16_t)row->curvature;
        context->mountain_uplift[target] = (int16_t)row->uplift;
        context->ocean_distance[target] = (int16_t)row->ocean_distance;
        context->moisture[target] = (int16_t)row->moisture;
        context->climate[target] = (uint8_t)row->climate;
        context->river_flags[target] =
            (uint16_t)(row->river_flags | WORLD_GEN_RIVER_LAKE);
        underlying = world_gen_classify_underlying_land(
            context, target, target % context->width, target / context->width,
            row->climate);
        lake_ok = underlying == row->expected &&
                  world_gen_classify_final(context) &&
                  context->geography[target] == GEO_LAKE;
        context->river_flags[target] = row->river_flags;
        restored_ok = world_gen_classify_final(context) &&
            context->geography[target] == row->expected &&
            world_gen_classify_underlying_land(
                context, target, target % context->width,
                target / context->width,
                (Climate)context->climate[target]) == row->expected;
        fprintf(file,
                "case=landform_rejected_lake_restore category=%s expected=%d "
                "underlying=%d lake_override=%d restored=%d ok=%d\n",
                row->label, row->expected, underlying, lake_ok, restored_ok,
                lake_ok && restored_ok);
        ok &= lake_ok && restored_ok;
    }
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
    int oasis_ok;
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
    context.moisture[target] = 43;
    context.climate[target] = CLIMATE_DESERT;
    context.river_flags[target] = WORLD_GEN_RIVER_CHANNEL;
    oasis_ok = world_gen_classify_final(&context) &&
               context.geography[target] == GEO_OASIS &&
               context.ecology[target] == ECO_GRASSLAND;
    context.moisture[target] = 42;
    oasis_ok &= world_gen_classify_final(&context) &&
                context.geography[target] != GEO_OASIS;
    context.moisture[target] = 43;
    context.river_flags[target] = 0;
    oasis_ok &= world_gen_classify_final(&context) &&
                context.geography[target] != GEO_OASIS;
    context.river_flags[target] = WORLD_GEN_RIVER_CHANNEL;
    context.climate[target] = CLIMATE_CONTINENTAL;
    oasis_ok &= world_gen_classify_final(&context) &&
                context.geography[target] != GEO_OASIS;
    restoration_ok &=
        rejected_lake_restoration_categories_contract(file, &context);
    fprintf(file,
            "case=landform_classify_fixture wet_bias_0=%d wet_bias_50=%d "
            "wet_bias_100=%d wet_expected=1/2/3 nondecreasing=%d "
            "lake_restoration=%d oasis_derived=%d ok=%d\n",
            wet_counts[0], wet_counts[1], wet_counts[2],
            wet_counts[0] <= wet_counts[1] && wet_counts[1] <= wet_counts[2],
            restoration_ok, oasis_ok, wet_ok && restoration_ok && oasis_ok);
    world_gen_context_destroy(&context);
    return wet_ok && restoration_ok && oasis_ok;
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
