#include "world_gen_classify.h"

#include "core/world_types.h"
#include "world/world_gen_aridity_projection.h"
#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify_climate.h"
#include "world/world_gen_rng.h"

#define WORLD_GEN_DESERT_BASE_DEFAULT 10
#define WORLD_GEN_DESERT_BIAS_SPAN_DEFAULT 2
#define WORLD_GEN_SEMI_ARID_WIDTH_DEFAULT 14
#define WORLD_GEN_OASIS_TRANSITION_MARGIN_DEFAULT 15
#define WORLD_GEN_VALIDATION_COEFFICIENT_MAX 100

static int validation_aridity_enabled;
static int validation_desert_base_value = WORLD_GEN_DESERT_BASE_DEFAULT;
static int validation_desert_bias_span_value = WORLD_GEN_DESERT_BIAS_SPAN_DEFAULT;
static int validation_semi_arid_width_value = WORLD_GEN_SEMI_ARID_WIDTH_DEFAULT;
static int validation_oasis_transition_margin_value =
    WORLD_GEN_OASIS_TRANSITION_MARGIN_DEFAULT;

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

int world_gen_desert_base(void) {
    return validation_aridity_enabled
        ? validation_desert_base_value
        : WORLD_GEN_DESERT_BASE_DEFAULT;
}

int world_gen_desert_bias_span(void) {
    return validation_aridity_enabled
        ? validation_desert_bias_span_value
        : WORLD_GEN_DESERT_BIAS_SPAN_DEFAULT;
}

int world_gen_semi_arid_width(void) {
    return validation_aridity_enabled
        ? validation_semi_arid_width_value
        : WORLD_GEN_SEMI_ARID_WIDTH_DEFAULT;
}

int world_gen_oasis_transition_margin(void) {
    return validation_aridity_enabled
        ? validation_oasis_transition_margin_value
        : WORLD_GEN_OASIS_TRANSITION_MARGIN_DEFAULT;
}

int world_gen_classify_validation_set_aridity(
    int desert_base, int bias_span, int semi_arid_width,
    int oasis_transition_margin) {
    if (desert_base <= 0 ||
        desert_base > WORLD_GEN_VALIDATION_COEFFICIENT_MAX ||
        bias_span <= 0 ||
        bias_span > WORLD_GEN_VALIDATION_COEFFICIENT_MAX ||
        semi_arid_width <= 0 ||
        semi_arid_width > WORLD_GEN_VALIDATION_COEFFICIENT_MAX ||
        oasis_transition_margin < 0 ||
        oasis_transition_margin > WORLD_GEN_VALIDATION_COEFFICIENT_MAX) return 0;
    validation_desert_base_value = desert_base;
    validation_desert_bias_span_value = bias_span;
    validation_semi_arid_width_value = semi_arid_width;
    validation_oasis_transition_margin_value = oasis_transition_margin;
    validation_aridity_enabled = 1;
    return 1;
}

void world_gen_classify_validation_reset_aridity(void) {
    validation_desert_base_value = WORLD_GEN_DESERT_BASE_DEFAULT;
    validation_desert_bias_span_value = WORLD_GEN_DESERT_BIAS_SPAN_DEFAULT;
    validation_semi_arid_width_value = WORLD_GEN_SEMI_ARID_WIDTH_DEFAULT;
    validation_oasis_transition_margin_value =
        WORLD_GEN_OASIS_TRANSITION_MARGIN_DEFAULT;
    validation_aridity_enabled = 0;
}

int world_gen_classify_validation_aridity_active(void) {
    return validation_aridity_enabled;
}

int world_gen_classify_validation_set_desert_bias_span(int bias_span) {
    return world_gen_classify_validation_set_aridity(
        WORLD_GEN_DESERT_BASE_DEFAULT, bias_span,
        WORLD_GEN_SEMI_ARID_WIDTH_DEFAULT,
        WORLD_GEN_OASIS_TRANSITION_MARGIN_DEFAULT);
}

void world_gen_classify_validation_reset_desert_bias_span(void) {
    world_gen_classify_validation_reset_aridity();
}

int world_gen_classify_validation_desert_bias_span_active(void) {
    return world_gen_classify_validation_aridity_active();
}

int world_gen_desert_moisture_limit(int bias_desert) {
    return world_gen_desert_base() +
        bias_desert * world_gen_desert_bias_span() / 100;
}

int world_gen_semi_arid_moisture_limit(int bias_desert) {
    return world_gen_desert_moisture_limit(bias_desert) +
        world_gen_semi_arid_width();
}

int world_gen_oasis_moisture_limit(int drought) {
    return 42 - drought * 18 / 100;
}

int world_gen_oasis_transition_moisture_limit_for_margin(
    int bias_desert, int drought, int oasis_transition_margin) {
    return world_gen_semi_arid_moisture_limit(bias_desert) +
        drought * oasis_transition_margin / 100;
}

int world_gen_oasis_transition_moisture_limit(int bias_desert, int drought) {
    return world_gen_oasis_transition_moisture_limit_for_margin(
        bias_desert, drought, world_gen_oasis_transition_margin());
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

static int legacy_oasis_predicate_for_limits(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_limit, int oasis_transition_limit) {
    int macro_arid;
    int dry_transition;
    if (!context || index < 0 || index >= context->tile_count ||
        !context->land_mask[index]) return 0;
    macro_arid = climate == CLIMATE_DESERT || climate == CLIMATE_SEMI_ARID;
    dry_transition = context->config.drought > 0 &&
        context->temperature[index] > 28 &&
        context->moisture[index] < oasis_transition_limit;
    return (context->river_flags[index] & WORLD_GEN_RIVER_CHANNEL) &&
        context->moisture[index] > oasis_limit &&
        (macro_arid || dry_transition);
}

static int response_oasis_predicate_for_limits(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_limit, int oasis_transition_limit) {
    return world_gen_classify_response_oasis_pair_independent_eligible(
            context, index, climate) &&
        world_gen_classify_response_oasis_moisture_in_window(
            context->config.drought, context->moisture[index], oasis_limit,
            oasis_transition_limit);
}

int world_gen_classify_oasis_predicate_for_margin(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_transition_margin) {
    if (oasis_transition_margin < 0 ||
        oasis_transition_margin > WORLD_GEN_VALIDATION_COEFFICIENT_MAX) return 0;
    return legacy_oasis_predicate_for_limits(
        context, index, climate,
        world_gen_oasis_moisture_limit(context ? context->config.drought : 0),
        context ? world_gen_oasis_transition_moisture_limit_for_margin(
            context->config.bias_desert, context->config.drought,
            oasis_transition_margin) : 0);
}

int world_gen_classify_response_oasis_predicate_for_pair(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_drop, int transition_margin) {
    WorldGenAridityResponseLimits limits;
    if (!context ||
        !world_gen_aridity_response_calculate_diminishing(
            context->config.moisture, context->config.drought,
            context->config.bias_desert,
            world_gen_aridity_response_arid_base(),
            world_gen_aridity_response_desert_bias_span(),
            world_gen_aridity_response_drought_classification_span(),
            world_gen_aridity_response_moisture_compression_span(),
            oasis_drop, transition_margin, &limits)) return 0;
    return response_oasis_predicate_for_limits(
        context, index, climate, limits.oasis_limit,
        limits.oasis_transition_limit);
}

static int qualifies_oasis(const WorldGenContext *context, int index,
                           Climate climate) {
    if (world_gen_aridity_response_validation_active() ||
        !world_gen_classify_validation_aridity_active()) {
        return world_gen_classify_response_oasis_predicate_for_pair(
            context, index, climate, world_gen_aridity_response_oasis_drop(),
            world_gen_aridity_response_transition_margin());
    }
    return world_gen_classify_oasis_predicate_for_margin(
        context, index, climate, world_gen_oasis_transition_margin());
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

static int oasis_visible_by_precedence(
    const WorldGenContext *context, int index) {
    if (!context || index < 0 || index >= context->tile_count ||
        !context->land_mask[index] ||
         (context->river_flags[index] &
         (WORLD_GEN_RIVER_DELTA | WORLD_GEN_RIVER_LAKE))) return 0;
    if (qualifies_coastal_lowland(context, index)) {
        if (qualifies_wetland(context, index)) return 0;
    } else if (context->ocean_distance[index] <= 1) {
        return 0;
    }
    return 1;
}

int world_gen_classify_response_oasis_pair_independent_eligible(
    const WorldGenContext *context, int index, Climate climate) {
    int macro_arid;
    if (!context || index < 0 || index >= context->tile_count ||
        !context->land_mask[index] ||
        !(context->river_flags[index] & WORLD_GEN_RIVER_CHANNEL)) return 0;
    macro_arid = climate == CLIMATE_DESERT || climate == CLIMATE_SEMI_ARID;
    return macro_arid || (context->config.drought > 0 &&
        context->temperature[index] > 28);
}

int world_gen_classify_response_oasis_visible_eligible(
    const WorldGenContext *context, int index, Climate climate) {
    return world_gen_classify_response_oasis_pair_independent_eligible(
            context, index, climate) &&
        oasis_visible_by_precedence(context, index);
}

int world_gen_classify_response_oasis_moisture_in_window(
    int drought, int moisture, int oasis_limit, int oasis_transition_limit) {
    return moisture > oasis_limit &&
        (drought <= 0 || moisture < oasis_transition_limit);
}

int world_gen_classify_visible_oasis_for_margin(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_transition_margin) {
    if (!oasis_visible_by_precedence(context, index)) return 0;
    return world_gen_classify_oasis_predicate_for_margin(
        context, index, climate, oasis_transition_margin);
}

int world_gen_classify_response_visible_oasis_for_pair(
    const WorldGenContext *context, int index, Climate climate,
    int oasis_drop, int transition_margin) {
    WorldGenAridityResponseLimits limits;
    if (!context || !world_gen_classify_response_oasis_visible_eligible(
            context, index, climate) ||
        !world_gen_aridity_response_calculate_diminishing(
            context->config.moisture, context->config.drought,
            context->config.bias_desert,
            world_gen_aridity_response_arid_base(),
            world_gen_aridity_response_desert_bias_span(),
            world_gen_aridity_response_drought_classification_span(),
            world_gen_aridity_response_moisture_compression_span(),
            oasis_drop, transition_margin, &limits)) return 0;
    return world_gen_classify_response_oasis_moisture_in_window(
        context->config.drought, context->moisture[index],
        limits.oasis_limit, limits.oasis_transition_limit);
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
    int projection_active;
    if (!context) return 0;
    projection_active = world_gen_aridity_projection_validation_active();
    for (y = 0; y < context->height; y++) {
        for (x = 0; x < context->width; x++) {
            int index = world_gen_context_index(context, x, y);
            int refresh = world_gen_classify_climate_refreshes_after_hydrology(
                context, index);
            Climate pre_climate;
            Climate climate;
            Geography geography;
            Ecology ecology;
            ResourceFeature resource;
            context->resource_variation[index] = (uint8_t)(world_gen_hash_u32(
                context->phase_seed[WORLD_GEN_PHASE_CLASSIFY] + 73u, (uint32_t)index) % 101u);
            pre_climate = (Climate)context->climate[index];
            climate = pre_climate;
            if (refresh) {
                climate = world_gen_classify_climate(context, index);
            }
            if (projection_active &&
                !world_gen_aridity_projection_capture_post(
                    context, index, refresh, pre_climate, climate)) return 0;
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
    int projection_active = world_gen_aridity_projection_validation_active();
    if (!context) return 0;
    if (projection_active &&
        !world_gen_aridity_projection_capture_begin_context(context)) return 0;
    for (i = 0; i < context->tile_count; i++) {
        context->climate[i] = (uint8_t)(context->land_mask[i]
            ? world_gen_classify_climate(context, i) : CLIMATE_OCEANIC);
        if (projection_active) {
            int eligible = context->land_mask[i] &&
                world_gen_classify_climate_arid_eligible(
                    context->relative_altitude[i], context->temperature[i]);
            if (!world_gen_aridity_projection_capture_pre(
                    context, i, eligible,
                    (Climate)context->climate[i])) return 0;
        }
    }
    return 1;
}
