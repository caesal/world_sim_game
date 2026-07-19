#include "world_gen_classify.h"

#include "core/world_types.h"
#include "world/world_gen_rng.h"

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int nearby_land(const WorldGenContext *context, int x, int y) {
    int count = 0;
    int dy;
    int dx;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if ((dx == 0 && dy == 0) || !world_gen_context_in_bounds(context, nx, ny)) continue;
            if (context->land_mask[world_gen_context_index(context, nx, ny)]) count++;
        }
    }
    return count;
}

static int large_lake_footprint(const WorldGenContext *context, int index) {
    int center_x;
    int center_y;
    int lake_tiles = 0;
    int dx;
    int dy;
    if (!(context->river_flags[index] & WORLD_GEN_RIVER_LAKE)) return 0;
    center_x = index % context->width;
    center_y = index / context->width;
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            int x = center_x + dx;
            int y = center_y + dy;
            if (!world_gen_context_in_bounds(context, x, y)) continue;
            if (context->river_flags[world_gen_context_index(context, x, y)] &
                WORLD_GEN_RIVER_LAKE) lake_tiles++;
        }
    }
    return lake_tiles >= 6;
}

static Climate classify_climate(const WorldGenContext *context, int index) {
    int elevation = context->relative_altitude[index];
    int moisture = context->moisture[index];
    int temperature = context->temperature[index];
    int near_ocean = context->ocean_distance[index] <= 18;
    int desert_limit = 6 + (context->config.drought + context->config.bias_desert) * 36 / 100;
    int semi_arid_limit = desert_limit + 16;
    if (elevation > 36 && temperature < 48) return CLIMATE_ALPINE;
    if (elevation > 28 && temperature < 68) return CLIMATE_HIGHLAND_PLATEAU;
    if (temperature < 12) return CLIMATE_ICE_CAP;
    if (temperature < 25) return CLIMATE_TUNDRA;
    if (temperature < 36) return CLIMATE_SUBARCTIC;
    if (moisture < desert_limit && temperature > 32) return CLIMATE_DESERT;
    if (moisture < semi_arid_limit && temperature > 28) return CLIMATE_SEMI_ARID;
    if (temperature > 72 && moisture > 78) return CLIMATE_TROPICAL_RAINFOREST;
    if (temperature > 68 && moisture > 58) return CLIMATE_TROPICAL_MONSOON;
    if (temperature > 64) return CLIMATE_TROPICAL_SAVANNA;
    if (near_ocean && temperature > 47 && moisture > 38 && moisture < 70) return CLIMATE_MEDITERRANEAN;
    if (near_ocean && moisture > 52) return CLIMATE_OCEANIC;
    if (moisture > 66 && temperature > 42) return CLIMATE_TEMPERATE_MONSOON;
    return CLIMATE_CONTINENTAL;
}

static int qualifies_oasis(const WorldGenContext *context, int index,
                           Climate climate) {
    return (climate == CLIMATE_DESERT || climate == CLIMATE_SEMI_ARID) &&
        (context->river_flags[index] & WORLD_GEN_RIVER_CHANNEL) &&
        context->moisture[index] > 42;
}

static int qualifies_wetland(const WorldGenContext *context, int index) {
    return context->moisture[index] >
            82 - (context->config.moisture + context->config.bias_wetland) / 8 &&
        context->slope[index] <= 5 && context->relative_altitude[index] < 18;
}

static int qualifies_coastal_lowland(const WorldGenContext *context,
                                     int index) {
    return context->coastal_lowland_hint[index] &&
           context->relative_altitude[index] <= 12 &&
           context->slope[index] <= 8;
}

Geography world_gen_classify_underlying_land(const WorldGenContext *context,
                                              int index, int x, int y,
                                              Climate climate) {
    int relative;
    int slope;
    int curvature;
    int uplift;
    int mountain_bias;
    uint16_t river_flags;
    uint32_t variation;

    if (!context || index < 0 || index >= context->tile_count ||
        !context->land_mask[index]) return GEO_OCEAN;
    relative = context->relative_altitude[index];
    slope = context->slope[index];
    curvature = context->curvature[index];
    uplift = context->mountain_uplift[index];
    mountain_bias = context->config.bias_mountain;
    river_flags = context->river_flags[index];
    variation = world_gen_hash_u32(
        context->phase_seed[WORLD_GEN_PHASE_CLASSIFY], (uint32_t)index);
    if (river_flags & WORLD_GEN_RIVER_DELTA) return GEO_DELTA;
    if (qualifies_coastal_lowland(context, index)) {
        if (qualifies_wetland(context, index)) return GEO_WETLAND;
        if (qualifies_oasis(context, index, climate)) return GEO_OASIS;
        if (context->ocean_distance[index] <= 1) return GEO_COAST;
        return GEO_PLAIN;
    }
    if (context->ocean_distance[index] <= 1) {
        if (nearby_land(context, x, y) <= 3) return GEO_ISLAND;
        return GEO_COAST;
    }
    if (qualifies_oasis(context, index, climate)) return GEO_OASIS;
    if (qualifies_wetland(context, index)) return GEO_WETLAND;
    if (relative >= 39 - mountain_bias / 18 && uplift >= 18 &&
        (slope >= 5 || uplift >= 30 || curvature >= 2)) {
        if ((variation & 1023u) < (uint32_t)(3 + mountain_bias / 5) && curvature > 2) return GEO_VOLCANO;
        return GEO_MOUNTAIN;
    }
    if ((river_flags & WORLD_GEN_RIVER_CHANNEL) && slope >= 14 && curvature <= -3 &&
        (variation & 7u) == 0u) return GEO_CANYON;
    if (relative >= 29 - mountain_bias / 24 && slope <= 7 && uplift >= 7) return GEO_PLATEAU;
    if (relative >= 18 - mountain_bias / 32 || slope >= 6 || uplift >= 7) return GEO_HILL;
    if (relative <= 9 && curvature <= -2) return GEO_BASIN;
    return GEO_PLAIN;
}

static Geography classify_geography(const WorldGenContext *context, int index,
                                     int x, int y, Climate climate) {
    Geography underlying;
    if (!context->land_mask[index]) {
        if (nearby_land(context, x, y) >= 5 &&
            context->elevation[index] >= context->sea_level - 5) return GEO_BAY;
        return GEO_OCEAN;
    }
    underlying = world_gen_classify_underlying_land(
        context, index, x, y, climate);
    if (context->river_flags[index] & WORLD_GEN_RIVER_LAKE) return GEO_LAKE;
    return underlying;
}

static Ecology classify_ecology(const WorldGenContext *context, int index,
                                Geography geography, Climate climate) {
    int moisture = context->moisture[index];
    int temperature = context->temperature[index];
    int forest_bias = (context->config.vegetation - 50) / 2 +
                      (context->config.bias_forest - 50) / 3;
    uint32_t variation = world_gen_hash_u32(context->phase_seed[WORLD_GEN_PHASE_CLASSIFY] + 19u,
                                            (uint32_t)index);
    if (geography == GEO_OCEAN || geography == GEO_BAY || geography == GEO_LAKE) return ECO_NONE;
    if (geography == GEO_DELTA && temperature > 65) return ECO_MANGROVE;
    if ((geography == GEO_WETLAND || geography == GEO_BASIN) && moisture > 68) return ECO_SWAMP;
    if (geography == GEO_OASIS) return ECO_GRASSLAND;
    switch (climate) {
        case CLIMATE_TROPICAL_RAINFOREST:
            return moisture + forest_bias > 82 && temperature > 72 ? ECO_RAINFOREST : ECO_FOREST;
        case CLIMATE_TROPICAL_MONSOON:
            if (moisture > 72 && (variation % 100u) < 24u) return ECO_BAMBOO;
            return moisture + forest_bias > 64 ? ECO_FOREST : ECO_GRASSLAND;
        case CLIMATE_TROPICAL_SAVANNA:
        case CLIMATE_SEMI_ARID:
            return ECO_GRASSLAND;
        case CLIMATE_DESERT:
            return ECO_DESERT;
        case CLIMATE_MEDITERRANEAN:
        case CLIMATE_CONTINENTAL:
            return moisture + forest_bias > 64 ? ECO_FOREST : ECO_GRASSLAND;
        case CLIMATE_OCEANIC:
        case CLIMATE_TEMPERATE_MONSOON:
            return moisture + forest_bias > 72 ? ECO_FOREST : ECO_GRASSLAND;
        case CLIMATE_SUBARCTIC:
            return moisture + forest_bias > 48 ? ECO_FOREST : ECO_TUNDRA;
        case CLIMATE_TUNDRA:
        case CLIMATE_ICE_CAP:
        case CLIMATE_ALPINE:
            return ECO_TUNDRA;
        case CLIMATE_HIGHLAND_PLATEAU:
            return moisture > 58 ? ECO_GRASSLAND : ECO_TUNDRA;
        case CLIMATE_COUNT:
            break;
    }
    return ECO_NONE;
}

static ResourceFeature classify_resource(const WorldGenContext *context, int index,
                                         Geography geography, Climate climate, Ecology ecology) {
    int moisture = context->moisture[index];
    int temperature = context->temperature[index];
    int elevation = context->elevation[index];
    int variation = context->resource_variation[index];
    if (geography == GEO_OCEAN) return RESOURCE_FEATURE_NONE;
    if (context->river_flags[index] & WORLD_GEN_RIVER_SALT_LAKE) {
        return RESOURCE_FEATURE_SALT_LAKE;
    }
    if (geography == GEO_BAY || geography == GEO_LAKE) return RESOURCE_FEATURE_FISHERY;
    if (geography == GEO_VOLCANO && variation > 60) return RESOURCE_FEATURE_GEOTHERMAL;
    if ((geography == GEO_MOUNTAIN || geography == GEO_HILL || geography == GEO_VOLCANO ||
         geography == GEO_CANYON || elevation > 78) && variation > 28) return RESOURCE_FEATURE_MINE;
    if ((geography == GEO_PLAIN || geography == GEO_DELTA || geography == GEO_BASIN ||
         geography == GEO_COAST || geography == GEO_OASIS) && moisture > 42 &&
        climate != CLIMATE_DESERT && climate != CLIMATE_ICE_CAP && climate != CLIMATE_TUNDRA &&
        variation > 18 && context->soil_fertility[index] >= 42) return RESOURCE_FEATURE_FARMLAND;
    if ((geography == GEO_COAST || geography == GEO_ISLAND) && variation > 42) {
        return RESOURCE_FEATURE_FISHERY;
    }
    if ((ecology == ECO_GRASSLAND || climate == CLIMATE_SEMI_ARID ||
         climate == CLIMATE_HIGHLAND_PLATEAU) && context->soil_fertility[index] >= 24) {
        return RESOURCE_FEATURE_PASTURE;
    }
    if (ecology == ECO_FOREST || ecology == ECO_RAINFOREST || ecology == ECO_BAMBOO ||
        ecology == ECO_MANGROVE) return RESOURCE_FEATURE_FOREST;
    if (temperature > 68 && moisture > 76 && geography == GEO_DELTA) return RESOURCE_FEATURE_FISHERY;
    return RESOURCE_FEATURE_NONE;
}

static int base_fertility(const WorldGenContext *context, int index, Geography geography) {
    int value = context->moisture[index] * 3 / 5 + context->temperature[index] / 5 -
                context->slope[index] * 2;
    if (geography == GEO_PLAIN || geography == GEO_BASIN) value += 10;
    if (geography == GEO_DELTA) value += 28;
    if (geography == GEO_MOUNTAIN || geography == GEO_VOLCANO) value -= 22;
    return clamp_int(value, 0, 100);
}

int world_gen_classify_final(WorldGenContext *context) {
    int y;
    int x;
    if (!context) return 0;
    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            int index = world_gen_context_index(context, x, y);
            Climate climate;
            Geography geography;
            Ecology ecology;
            ResourceFeature resource;
            context->resource_variation[index] = (uint8_t)(world_gen_hash_u32(
                context->phase_seed[WORLD_GEN_PHASE_CLASSIFY] + 73u, (uint32_t)index) % 101u);
            climate = (Climate)context->climate[index];
            if (context->land_mask[index] &&
                ((context->river_flags[index] & WORLD_GEN_RIVER_DELTA) ||
                 large_lake_footprint(context, index))) {
                climate = classify_climate(context, index);
            }
            geography = classify_geography(context, index, x, y, climate);
            ecology = classify_ecology(context, index, geography, climate);
            if (context->soil_fertility[index] < base_fertility(context, index, geography)) {
                context->soil_fertility[index] = (uint8_t)base_fertility(context, index, geography);
            }
            resource = classify_resource(context, index, geography, climate, ecology);
            context->climate[index] = (uint8_t)climate;
            context->geography[index] = (uint8_t)geography;
            context->ecology[index] = (uint8_t)ecology;
            context->resource[index] = (uint8_t)resource;
        }
    }
    return 1;
}

int world_gen_classify_macro_climate(WorldGenContext *context) {
    int i;
    if (!context) return 0;
    for (i = 0; i < context->tile_count; i++) {
        context->climate[i] = (uint8_t)(context->land_mask[i]
            ? classify_climate(context, i) : CLIMATE_OCEANIC);
    }
    return 1;
}
