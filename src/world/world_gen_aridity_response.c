#include "world/world_gen_aridity_response.h"

#include <stdint.h>

enum {
    RESPONSE_INPUT_MIN = 0,
    RESPONSE_INPUT_MAX = 100,
    RESPONSE_LEGACY_ARID_BASE = 30,
    RESPONSE_LEGACY_DESERT_BIAS_SPAN = 2,
    RESPONSE_PRODUCTION_ARID_BASE = 31,
    RESPONSE_PRODUCTION_DESERT_BIAS_SPAN = 4,
    RESPONSE_PRODUCTION_DROUGHT_CLASSIFICATION_SPAN = 2,
    RESPONSE_PRODUCTION_MOISTURE_COMPRESSION_SPAN = 12,
    RESPONSE_PRODUCTION_OASIS_DROP = 20,
    RESPONSE_PRODUCTION_TRANSITION_MARGIN = 2,
    RESPONSE_COMPRESSION_MIN = 1,
    RESPONSE_COEFFICIENT_MAX = 100
};

static int validation_enabled;
static int validation_arid_base;
static int validation_desert_bias_span;
static int validation_drought_classification_span;
static int validation_moisture_compression_span;
static int validation_oasis_drop;
static int validation_transition_margin;

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int max_int(int left, int right) {
    return left > right ? left : right;
}

static int response_dimensions_valid(
    int arid_base, int desert_bias_span, int drought_classification_span,
    int moisture_compression_span, int oasis_drop, int transition_margin) {
    return arid_base >= RESPONSE_INPUT_MIN &&
        arid_base <= RESPONSE_COEFFICIENT_MAX &&
        desert_bias_span >= RESPONSE_INPUT_MIN &&
        desert_bias_span <= RESPONSE_COEFFICIENT_MAX &&
        drought_classification_span >= RESPONSE_INPUT_MIN &&
        drought_classification_span <= RESPONSE_COEFFICIENT_MAX &&
        moisture_compression_span >= RESPONSE_COMPRESSION_MIN &&
        moisture_compression_span <= RESPONSE_COEFFICIENT_MAX &&
        oasis_drop >= RESPONSE_INPUT_MIN &&
        oasis_drop <= RESPONSE_COEFFICIENT_MAX &&
        transition_margin >= RESPONSE_INPUT_MIN &&
        transition_margin <= RESPONSE_COEFFICIENT_MAX;
}

int world_gen_aridity_response_calculate(
    int world_moisture, int drought, int bias_desert,
    int moisture_compression_span, int oasis_drop, int transition_margin,
    WorldGenAridityResponseLimits *limits) {
    return world_gen_aridity_response_calculate_expanded(
        world_moisture, drought, bias_desert, RESPONSE_LEGACY_ARID_BASE,
        RESPONSE_LEGACY_DESERT_BIAS_SPAN, moisture_compression_span,
        oasis_drop, transition_margin, limits);
}

int world_gen_aridity_response_calculate_expanded(
    int world_moisture, int drought, int bias_desert, int arid_base,
    int desert_bias_span, int moisture_compression_span, int oasis_drop,
    int transition_margin, WorldGenAridityResponseLimits *limits) {
    return world_gen_aridity_response_calculate_diminishing(
        world_moisture, drought, bias_desert, arid_base,
        desert_bias_span, 0, moisture_compression_span, oasis_drop,
        transition_margin, limits);
}

int world_gen_aridity_response_calculate_diminishing(
    int world_moisture, int drought, int bias_desert, int arid_base,
    int desert_bias_span, int drought_classification_span,
    int moisture_compression_span, int oasis_drop, int transition_margin,
    WorldGenAridityResponseLimits *limits) {
    int64_t bias_numerator;
    int64_t drought_numerator;
    int dryness_response;
    int combined_arid_limit;
    int semi_arid_band;
    if (!limits || world_moisture < RESPONSE_INPUT_MIN ||
        world_moisture > RESPONSE_INPUT_MAX || drought < RESPONSE_INPUT_MIN ||
        drought > RESPONSE_INPUT_MAX || bias_desert < RESPONSE_INPUT_MIN ||
        bias_desert > RESPONSE_INPUT_MAX ||
        !response_dimensions_valid(arid_base, desert_bias_span,
            drought_classification_span, moisture_compression_span,
            oasis_drop, transition_margin)) return 0;
    bias_numerator = (int64_t)bias_desert * desert_bias_span * 100;
    drought_numerator = (int64_t)drought * drought_classification_span *
        (100 - bias_desert);
    dryness_response = (int)((bias_numerator + drought_numerator) / 10000);
    combined_arid_limit = clamp_int(
        arid_base + dryness_response +
            (world_moisture - 50) * moisture_compression_span / 25,
        0, 100);
    semi_arid_band = clamp_int(
        12 - bias_desert * 10 / 100 +
            max_int(0, 50 - world_moisture) * 8 / 25,
        2, 20);
    limits->combined_arid_limit = combined_arid_limit;
    limits->semi_arid_band = semi_arid_band;
    limits->desert_limit = clamp_int(
        combined_arid_limit - semi_arid_band, 0, 100);
    limits->semi_arid_limit = combined_arid_limit;
    limits->oasis_limit = 42 - drought * oasis_drop / 100;
    limits->oasis_transition_limit = combined_arid_limit +
        drought * transition_margin / 100;
    return 1;
}

int world_gen_aridity_response_validation_enable(
    int moisture_compression_span, int oasis_drop, int transition_margin) {
    return world_gen_aridity_response_validation_enable_expanded(
        RESPONSE_LEGACY_ARID_BASE, RESPONSE_LEGACY_DESERT_BIAS_SPAN,
        moisture_compression_span, oasis_drop, transition_margin);
}

int world_gen_aridity_response_validation_enable_expanded(
    int arid_base, int desert_bias_span, int moisture_compression_span,
    int oasis_drop, int transition_margin) {
    return world_gen_aridity_response_validation_enable_diminishing(
        arid_base, desert_bias_span, 0, moisture_compression_span,
        oasis_drop, transition_margin);
}

int world_gen_aridity_response_validation_enable_diminishing(
    int arid_base, int desert_bias_span, int drought_classification_span,
    int moisture_compression_span, int oasis_drop, int transition_margin) {
    world_gen_aridity_response_validation_reset();
    if (!response_dimensions_valid(arid_base, desert_bias_span,
            drought_classification_span, moisture_compression_span,
            oasis_drop, transition_margin)) return 0;
    validation_arid_base = arid_base;
    validation_desert_bias_span = desert_bias_span;
    validation_drought_classification_span = drought_classification_span;
    validation_moisture_compression_span = moisture_compression_span;
    validation_oasis_drop = oasis_drop;
    validation_transition_margin = transition_margin;
    validation_enabled = 1;
    return 1;
}

void world_gen_aridity_response_validation_reset(void) {
    validation_arid_base = 0;
    validation_desert_bias_span = 0;
    validation_drought_classification_span = 0;
    validation_moisture_compression_span = 0;
    validation_oasis_drop = 0;
    validation_transition_margin = 0;
    validation_enabled = 0;
}

int world_gen_aridity_response_validation_active(void) {
    return validation_enabled;
}

int world_gen_aridity_response_validation_matches(
    int moisture_compression_span, int oasis_drop, int transition_margin) {
    return world_gen_aridity_response_validation_matches_expanded(
        RESPONSE_LEGACY_ARID_BASE, RESPONSE_LEGACY_DESERT_BIAS_SPAN,
        moisture_compression_span, oasis_drop, transition_margin);
}

int world_gen_aridity_response_validation_matches_expanded(
    int arid_base, int desert_bias_span, int moisture_compression_span,
    int oasis_drop, int transition_margin) {
    return world_gen_aridity_response_validation_matches_diminishing(
        arid_base, desert_bias_span, 0, moisture_compression_span,
        oasis_drop, transition_margin);
}

int world_gen_aridity_response_validation_matches_diminishing(
    int arid_base, int desert_bias_span, int drought_classification_span,
    int moisture_compression_span, int oasis_drop, int transition_margin) {
    return validation_enabled && validation_arid_base == arid_base &&
        validation_desert_bias_span == desert_bias_span &&
        validation_drought_classification_span ==
            drought_classification_span &&
        validation_moisture_compression_span == moisture_compression_span &&
        validation_oasis_drop == oasis_drop &&
        validation_transition_margin == transition_margin;
}

int world_gen_aridity_response_arid_base(void) {
    return validation_enabled ? validation_arid_base :
        RESPONSE_PRODUCTION_ARID_BASE;
}

int world_gen_aridity_response_desert_bias_span(void) {
    return validation_enabled ? validation_desert_bias_span :
        RESPONSE_PRODUCTION_DESERT_BIAS_SPAN;
}

int world_gen_aridity_response_drought_classification_span(void) {
    return validation_enabled ? validation_drought_classification_span :
        RESPONSE_PRODUCTION_DROUGHT_CLASSIFICATION_SPAN;
}

int world_gen_aridity_response_moisture_compression_span(void) {
    return validation_enabled ? validation_moisture_compression_span :
        RESPONSE_PRODUCTION_MOISTURE_COMPRESSION_SPAN;
}

int world_gen_aridity_response_oasis_drop(void) {
    return validation_enabled ? validation_oasis_drop :
        RESPONSE_PRODUCTION_OASIS_DROP;
}

int world_gen_aridity_response_transition_margin(void) {
    return validation_enabled ? validation_transition_margin :
        RESPONSE_PRODUCTION_TRANSITION_MARGIN;
}

int world_gen_aridity_response_current_limits(
    int world_moisture, int drought, int bias_desert,
    WorldGenAridityResponseLimits *limits) {
    return world_gen_aridity_response_calculate_diminishing(
        world_moisture, drought, bias_desert,
        world_gen_aridity_response_arid_base(),
        world_gen_aridity_response_desert_bias_span(),
        world_gen_aridity_response_drought_classification_span(),
        world_gen_aridity_response_moisture_compression_span(),
        world_gen_aridity_response_oasis_drop(),
        world_gen_aridity_response_transition_margin(), limits);
}
