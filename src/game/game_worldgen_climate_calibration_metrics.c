#include "game/game_worldgen_climate_calibration_metrics.h"

#include "world/world_gen_context.h"

#include <string.h>

typedef struct {
    uint64_t bins[101];
    ClimateCalibrationDistribution result;
} DistributionBuilder;

static int distribution_add(DistributionBuilder *builder, int value) {
    if (!builder || value < 0 || value > 100) return 0;
    builder->bins[value]++;
    builder->result.count++;
    builder->result.sum += (uint64_t)value;
    return 1;
}

static int percentile(const DistributionBuilder *builder, int percent) {
    uint64_t rank;
    uint64_t cumulative = 0;
    int value;
    if (!builder || builder->result.count <= 0) return -1;
    rank = ((uint64_t)percent * (uint64_t)builder->result.count + 99u) / 100u;
    if (rank < 1u) rank = 1u;
    for (value = 0; value <= 100; value++) {
        cumulative += builder->bins[value];
        if (cumulative >= rank) return value;
    }
    return -1;
}

static void distribution_finish(DistributionBuilder *builder) {
    int value;
    if (!builder) return;
    builder->result.minimum = -1;
    builder->result.maximum = -1;
    if (builder->result.count <= 0) return;
    for (value = 0; value <= 100; value++) {
        if (builder->bins[value] > 0) {
            builder->result.minimum = value;
            break;
        }
    }
    for (value = 100; value >= 0; value--) {
        if (builder->bins[value] > 0) {
            builder->result.maximum = value;
            break;
        }
    }
    builder->result.p10 = percentile(builder, 10);
    builder->result.p50 = percentile(builder, 50);
    builder->result.p90 = percentile(builder, 90);
}

static ClimateCalibrationDisplayGroup display_group(int climate, int ecology) {
    if (climate == CLIMATE_ICE_CAP) {
        return CLIMATE_CALIBRATION_GROUP_ICEFIELD;
    }
    if (climate == CLIMATE_TUNDRA || climate == CLIMATE_SUBARCTIC ||
        climate == CLIMATE_ALPINE || ecology == ECO_TUNDRA) {
        return CLIMATE_CALIBRATION_GROUP_TUNDRA;
    }
    if (climate == CLIMATE_TROPICAL_RAINFOREST || ecology == ECO_RAINFOREST) {
        return CLIMATE_CALIBRATION_GROUP_TROPICAL_RAINFOREST;
    }
    if (climate == CLIMATE_TROPICAL_MONSOON ||
        climate == CLIMATE_TEMPERATE_MONSOON) {
        return CLIMATE_CALIBRATION_GROUP_MONSOON;
    }
    if (climate == CLIMATE_DESERT || ecology == ECO_DESERT) {
        return CLIMATE_CALIBRATION_GROUP_DESERT;
    }
    if (ecology == ECO_FOREST || ecology == ECO_BAMBOO ||
        ecology == ECO_MANGROVE) {
        return CLIMATE_CALIBRATION_GROUP_FOREST;
    }
    if (ecology == ECO_GRASSLAND &&
        (climate == CLIMATE_SEMI_ARID ||
         climate == CLIMATE_MEDITERRANEAN ||
         climate == CLIMATE_OCEANIC ||
         climate == CLIMATE_CONTINENTAL ||
         climate == CLIMATE_HIGHLAND_PLATEAU)) {
        return CLIMATE_CALIBRATION_GROUP_TEMPERATE_GRASSLAND;
    }
    return CLIMATE_CALIBRATION_GROUP_OTHER_TRANSITION;
}

static uint64_t sum_counts(const uint64_t *counts, int count) {
    uint64_t sum = 0;
    int i;
    for (i = 0; i < count; i++) sum += counts[i];
    return sum;
}

int game_worldgen_climate_calibration_metrics_valid(
    const ClimateCalibrationMetrics *metrics) {
    uint64_t tiles;
    if (!metrics || metrics->tile_count <= 0 ||
        metrics->land_mask_nonbinary != 0 ||
        metrics->invalid_geography != 0 ||
        metrics->invalid_climate != 0 ||
        metrics->invalid_ecology != 0 ||
        metrics->invalid_continuous_value != 0) return 0;
    tiles = (uint64_t)metrics->tile_count;
    if ((uint64_t)metrics->land_mask_tiles +
            (uint64_t)metrics->ocean_tiles != tiles ||
        metrics->terrestrial_tiles + metrics->lake_tiles !=
            metrics->land_mask_tiles ||
        sum_counts(metrics->geography_counts, GEO_COUNT) != tiles ||
        sum_counts(metrics->climate_counts, CLIMATE_COUNT) != tiles ||
        sum_counts(metrics->ecology_counts, ECO_COUNT) != tiles ||
        sum_counts(metrics->display_group_counts,
                   CLIMATE_CALIBRATION_GROUP_COUNT) !=
            (uint64_t)metrics->terrestrial_tiles) return 0;
    if (metrics->temperature.count != metrics->terrestrial_tiles ||
        metrics->moisture.count != metrics->terrestrial_tiles ||
        metrics->precipitation.count != metrics->terrestrial_tiles) return 0;
    if (metrics->terrestrial_tiles <= 0) return 0;
    return metrics->temperature.minimum >= 0 &&
           metrics->temperature.maximum <= 100 &&
           metrics->moisture.minimum >= 0 &&
           metrics->moisture.maximum <= 100 &&
           metrics->precipitation.minimum >= 0 &&
           metrics->precipitation.maximum <= 100;
}

int game_worldgen_climate_calibration_collect_metrics(
    const WorldGenContext *context, ClimateCalibrationMetrics *out) {
    DistributionBuilder temperature = {0};
    DistributionBuilder moisture = {0};
    DistributionBuilder precipitation = {0};
    int i;
    if (!context || !out || context->tile_count <= 0 ||
        !context->land_mask || !context->geography || !context->climate ||
        !context->ecology || !context->temperature || !context->moisture ||
        !context->precipitation) return 0;
    memset(out, 0, sizeof(*out));
    out->tile_count = context->tile_count;
    for (i = 0; i < context->tile_count; i++) {
        int land = context->land_mask[i] != 0;
        int geography = context->geography[i];
        int climate = context->climate[i];
        int ecology = context->ecology[i];
        if (context->land_mask[i] > 1u) out->land_mask_nonbinary++;
        if (land) out->land_mask_tiles++;
        else out->ocean_tiles++;
        if (geography >= 0 && geography < GEO_COUNT) {
            out->geography_counts[geography]++;
        } else {
            out->invalid_geography++;
        }
        if (climate >= 0 && climate < CLIMATE_COUNT) {
            out->climate_counts[climate]++;
        } else {
            out->invalid_climate++;
        }
        if (ecology >= 0 && ecology < ECO_COUNT) {
            out->ecology_counts[ecology]++;
        } else {
            out->invalid_ecology++;
        }
        if (land && geography == GEO_LAKE) out->lake_tiles++;
        if (!land || geography == GEO_LAKE) continue;
        out->terrestrial_tiles++;
        if (!distribution_add(&temperature, context->temperature[i]))
            out->invalid_continuous_value++;
        if (!distribution_add(&moisture, context->moisture[i]))
            out->invalid_continuous_value++;
        if (!distribution_add(&precipitation, context->precipitation[i]))
            out->invalid_continuous_value++;
        if (climate >= 0 && climate < CLIMATE_COUNT &&
            ecology >= 0 && ecology < ECO_COUNT) {
            out->display_group_counts[display_group(climate, ecology)]++;
        }
    }
    distribution_finish(&temperature);
    distribution_finish(&moisture);
    distribution_finish(&precipitation);
    out->temperature = temperature.result;
    out->moisture = moisture.result;
    out->precipitation = precipitation.result;
    out->valid = game_worldgen_climate_calibration_metrics_valid(out);
    return out->valid;
}
