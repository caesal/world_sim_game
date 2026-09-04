#include "world/world_gen_classify_climate.h"

#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"

static int pre_arid_climate(int elevation, int temperature, Climate *climate) {
    if (elevation > 36 && temperature < 48) {
        *climate = CLIMATE_ALPINE;
        return 1;
    }
    if (elevation > 28 && temperature < 68) {
        *climate = CLIMATE_HIGHLAND_PLATEAU;
        return 1;
    }
    if (temperature < 12) {
        *climate = CLIMATE_ICE_CAP;
        return 1;
    }
    if (temperature < 25) {
        *climate = CLIMATE_TUNDRA;
        return 1;
    }
    if (temperature < 36) {
        *climate = CLIMATE_SUBARCTIC;
        return 1;
    }
    return 0;
}

int world_gen_classify_climate_arid_eligible(int elevation, int temperature) {
    Climate ignored;
    return !pre_arid_climate(elevation, temperature, &ignored) &&
        temperature > 32;
}

Climate world_gen_classify_climate(const WorldGenContext *context, int index) {
    int elevation = context->relative_altitude[index];
    int moisture = context->moisture[index];
    int temperature = context->temperature[index];
    int near_ocean = context->ocean_distance[index] <= 18;
    int desert_limit = world_gen_desert_moisture_limit(
        context->config.bias_desert);
    int semi_arid_limit = world_gen_semi_arid_moisture_limit(
        context->config.bias_desert);
    Climate climate;
    WorldGenAridityResponseLimits response_limits;

    if (!world_gen_classify_validation_aridity_active() &&
        world_gen_aridity_response_current_limits(
            context->config.moisture, context->config.drought,
            context->config.bias_desert, &response_limits)) {
        desert_limit = response_limits.desert_limit;
        semi_arid_limit = response_limits.semi_arid_limit;
    }
    if (pre_arid_climate(elevation, temperature, &climate)) return climate;
    if (moisture < desert_limit && temperature > 32) return CLIMATE_DESERT;
    if (moisture < semi_arid_limit && temperature > 28) {
        return CLIMATE_SEMI_ARID;
    }
    if (temperature > 72 && moisture > 78) {
        return CLIMATE_TROPICAL_RAINFOREST;
    }
    if (temperature > 68 && moisture > 58) return CLIMATE_TROPICAL_MONSOON;
    if (temperature > 64) return CLIMATE_TROPICAL_SAVANNA;
    if (near_ocean && temperature > 47 && moisture > 38 && moisture < 70) {
        return CLIMATE_MEDITERRANEAN;
    }
    if (near_ocean && moisture > 52) return CLIMATE_OCEANIC;
    if (moisture > 66 && temperature > 42) return CLIMATE_TEMPERATE_MONSOON;
    return CLIMATE_CONTINENTAL;
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

int world_gen_classify_climate_refreshes_after_hydrology(
    const WorldGenContext *context, int index) {
    return context && index >= 0 && index < context->tile_count &&
        context->land_mask[index] &&
        ((context->river_flags[index] & WORLD_GEN_RIVER_DELTA) ||
         large_lake_footprint(context, index));
}
