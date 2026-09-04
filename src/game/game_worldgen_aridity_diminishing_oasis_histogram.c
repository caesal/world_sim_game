#include "game/game_worldgen_aridity_diminishing_oasis_histogram.h"

#include "world/world_gen_aridity_response.h"
#include "world/world_gen_classify.h"
#include "world/world_gen_context.h"

#include <string.h>

int game_worldgen_aridity_diminishing_oasis_pair_valid(
    int oasis_drop, int transition_margin) {
    return oasis_drop >= 0 &&
        oasis_drop < GAME_WORLDGEN_ARIDITY_DIMINISHING_DROP_COUNT &&
        transition_margin >= 0 &&
        transition_margin < GAME_WORLDGEN_ARIDITY_DIMINISHING_MARGIN_COUNT;
}

void game_worldgen_aridity_diminishing_oasis_histogram_reset(
    GameWorldgenAridityDiminishingOasisHistogram *histogram) {
    if (histogram) memset(histogram, 0, sizeof(*histogram));
}

int game_worldgen_aridity_diminishing_oasis_histogram_note_tile(
    GameWorldgenAridityDiminishingOasisHistogram *histogram,
    const WorldGenContext *context, int index, Climate climate) {
    int moisture;
    if (!histogram || !context || histogram->finalized || index < 0 ||
        index >= context->tile_count) return 0;
    if (!world_gen_classify_response_oasis_visible_eligible(
            context, index, climate)) return 1;
    moisture = context->moisture[index];
    if (moisture < 0 ||
        moisture >= GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT) return 0;
    histogram->moisture_bins[moisture]++;
    histogram->eligible_tile_count++;
    return 1;
}

static int prefix_range_count(
    const GameWorldgenAridityDiminishingOasisHistogram *histogram,
    int first, int last) {
    int count;
    if (first < 0) first = 0;
    if (last >= GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT) {
        last = GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT - 1;
    }
    if (first > last || last < 0 ||
        first >= GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT) return 0;
    count = histogram->moisture_prefix[last];
    if (first > 0) count -= histogram->moisture_prefix[first - 1];
    return count;
}

static int projected_count(
    const GameWorldgenAridityDiminishingOasisHistogram *histogram,
    int drought, int oasis_limit, int oasis_transition_limit) {
    int first = oasis_limit + 1;
    int last = drought > 0 ? oasis_transition_limit - 1 : 100;
    return prefix_range_count(histogram, first, last);
}

static int oracle_count(
    GameWorldgenAridityDiminishingOasisHistogram *histogram,
    int drought, int oasis_limit, int oasis_transition_limit) {
    int moisture;
    int count = 0;
    for (moisture = 0;
         moisture < GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT;
         moisture++) {
        histogram->oracle_case_count++;
        if (world_gen_classify_response_oasis_moisture_in_window(
                drought, moisture, oasis_limit, oasis_transition_limit)) {
            count += histogram->moisture_bins[moisture];
        }
    }
    return count;
}

int game_worldgen_aridity_diminishing_oasis_histogram_finalize(
    GameWorldgenAridityDiminishingOasisHistogram *histogram,
    const WorldGenContext *context) {
    int moisture;
    int drop;
    int margin;
    if (!histogram || !context || histogram->finalized) return 0;
    for (moisture = 0;
         moisture < GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT;
         moisture++) {
        histogram->moisture_prefix[moisture] =
            histogram->moisture_bins[moisture] +
            (moisture > 0 ? histogram->moisture_prefix[moisture - 1] : 0);
    }
    for (drop = 0; drop < GAME_WORLDGEN_ARIDITY_DIMINISHING_DROP_COUNT;
         drop++) {
        for (margin = 0;
             margin < GAME_WORLDGEN_ARIDITY_DIMINISHING_MARGIN_COUNT;
             margin++) {
            WorldGenAridityResponseLimits limits;
            int prefix_count;
            int direct_count;
            if (!world_gen_aridity_response_calculate_diminishing(
                    context->config.moisture, context->config.drought,
                    context->config.bias_desert,
                    world_gen_aridity_response_arid_base(),
                    world_gen_aridity_response_desert_bias_span(),
                    world_gen_aridity_response_drought_classification_span(),
                    world_gen_aridity_response_moisture_compression_span(),
                    drop, margin, &limits)) return 0;
            prefix_count = projected_count(
                histogram, context->config.drought, limits.oasis_limit,
                limits.oasis_transition_limit);
            direct_count = oracle_count(
                histogram, context->config.drought, limits.oasis_limit,
                limits.oasis_transition_limit);
            histogram->projected_visible_oasis_count[drop][margin] =
                prefix_count;
            histogram->oracle_mismatch_count +=
                prefix_count != direct_count;
        }
    }
    histogram->finalized = 1;
    histogram->ok = histogram->oracle_case_count ==
            GAME_WORLDGEN_ARIDITY_DIMINISHING_PAIR_COUNT *
            GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT &&
        histogram->oracle_mismatch_count == 0 &&
        histogram->moisture_prefix[
            GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT - 1] ==
            histogram->eligible_tile_count;
    return histogram->ok;
}

int game_worldgen_aridity_diminishing_oasis_histogram_count(
    const GameWorldgenAridityDiminishingOasisHistogram *histogram,
    int oasis_drop, int transition_margin, int *count) {
    if (!histogram || !count || !histogram->finalized ||
        !game_worldgen_aridity_diminishing_oasis_pair_valid(
            oasis_drop, transition_margin)) return 0;
    *count = histogram->projected_visible_oasis_count
        [oasis_drop][transition_margin];
    return 1;
}

int game_worldgen_aridity_diminishing_oasis_histogram_crosscheck_legacy(
    GameWorldgenAridityDiminishingOasisHistogram *histogram,
    int oasis_drop, int transition_margin, int expected_count) {
    int actual;
    if (!histogram || expected_count < 0 ||
        !game_worldgen_aridity_diminishing_oasis_histogram_count(
            histogram, oasis_drop, transition_margin, &actual)) return 0;
    histogram->legacy_crosscheck_case_count++;
    histogram->legacy_crosscheck_mismatch_count += actual != expected_count;
    histogram->ok = histogram->ok && actual == expected_count;
    return actual == expected_count;
}

static int self_test_strict_endpoints(void) {
    return !world_gen_classify_response_oasis_moisture_in_window(
            100, 24, 24, 30) &&
        world_gen_classify_response_oasis_moisture_in_window(
            100, 25, 24, 30) &&
        world_gen_classify_response_oasis_moisture_in_window(
            100, 29, 24, 30) &&
        !world_gen_classify_response_oasis_moisture_in_window(
            100, 30, 24, 30) &&
        !world_gen_classify_response_oasis_moisture_in_window(
            0, 42, 42, 10) &&
        world_gen_classify_response_oasis_moisture_in_window(
            0, 43, 42, 10);
}

static int self_test_invalid_boundaries(void) {
    return game_worldgen_aridity_diminishing_oasis_pair_valid(0, 0) &&
        game_worldgen_aridity_diminishing_oasis_pair_valid(20, 25) &&
        !game_worldgen_aridity_diminishing_oasis_pair_valid(-1, 0) &&
        !game_worldgen_aridity_diminishing_oasis_pair_valid(21, 0) &&
        !game_worldgen_aridity_diminishing_oasis_pair_valid(0, -1) &&
        !game_worldgen_aridity_diminishing_oasis_pair_valid(0, 26);
}

int game_worldgen_aridity_diminishing_oasis_histogram_self_test(
    GameWorldgenAridityDiminishingOasisHistogramSelfTest *result) {
    GameWorldgenAridityDiminishingOasisHistogram histogram;
    int moisture;
    int drop;
    int margin;
    if (!result) return 0;
    memset(result, 0, sizeof(*result));
    game_worldgen_aridity_diminishing_oasis_histogram_reset(&histogram);
    for (moisture = 0;
         moisture < GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT;
         moisture++) {
        histogram.moisture_bins[moisture] = moisture % 7 + 1;
        histogram.eligible_tile_count += histogram.moisture_bins[moisture];
        histogram.moisture_prefix[moisture] =
            histogram.moisture_bins[moisture] +
            (moisture > 0 ? histogram.moisture_prefix[moisture - 1] : 0);
    }
    for (drop = 0; drop < GAME_WORLDGEN_ARIDITY_DIMINISHING_DROP_COUNT;
         drop++) {
        for (margin = 0;
             margin < GAME_WORLDGEN_ARIDITY_DIMINISHING_MARGIN_COUNT;
             margin++) {
            WorldGenAridityResponseLimits limits;
            int prefix_count;
            int direct_count = 0;
            if (!world_gen_aridity_response_calculate_diminishing(
                    50, 100, 100, 31, 4, 2, 12, drop, margin, &limits)) {
                return 0;
            }
            result->pair_cases++;
            prefix_count = projected_count(
                &histogram, 100, limits.oasis_limit,
                limits.oasis_transition_limit);
            for (moisture = 0;
                 moisture < GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT;
                 moisture++) {
                result->oracle_cases++;
                if (world_gen_classify_response_oasis_moisture_in_window(
                        100, moisture, limits.oasis_limit,
                        limits.oasis_transition_limit)) {
                    direct_count += histogram.moisture_bins[moisture];
                }
            }
            result->mismatch_count += prefix_count != direct_count;
        }
    }
    result->strict_endpoint_cases = 6;
    result->invalid_boundary_cases = 6;
    result->ok = result->pair_cases ==
            GAME_WORLDGEN_ARIDITY_DIMINISHING_PAIR_COUNT &&
        result->oracle_cases == GAME_WORLDGEN_ARIDITY_DIMINISHING_PAIR_COUNT *
            GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT &&
        result->mismatch_count == 0 && self_test_strict_endpoints() &&
        self_test_invalid_boundaries() &&
        histogram.moisture_prefix[
            GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT - 1] ==
            histogram.eligible_tile_count;
    return result->ok;
}
