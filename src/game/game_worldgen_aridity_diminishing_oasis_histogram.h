#ifndef WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_OASIS_HISTOGRAM_H
#define WORLD_SIM_GAME_WORLDGEN_ARIDITY_DIMINISHING_OASIS_HISTOGRAM_H

#include "core/world_types.h"

struct WorldGenContext;

enum {
    GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT = 101,
    GAME_WORLDGEN_ARIDITY_DIMINISHING_DROP_COUNT = 21,
    GAME_WORLDGEN_ARIDITY_DIMINISHING_MARGIN_COUNT = 26,
    GAME_WORLDGEN_ARIDITY_DIMINISHING_PAIR_COUNT = 546
};

typedef struct {
    int moisture_bins[GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT];
    int moisture_prefix[GAME_WORLDGEN_ARIDITY_DIMINISHING_MOISTURE_COUNT];
    int projected_visible_oasis_count
        [GAME_WORLDGEN_ARIDITY_DIMINISHING_DROP_COUNT]
        [GAME_WORLDGEN_ARIDITY_DIMINISHING_MARGIN_COUNT];
    int eligible_tile_count;
    int oracle_case_count;
    int oracle_mismatch_count;
    int legacy_crosscheck_case_count;
    int legacy_crosscheck_mismatch_count;
    int finalized;
    int ok;
} GameWorldgenAridityDiminishingOasisHistogram;

typedef struct {
    int pair_cases;
    int oracle_cases;
    int strict_endpoint_cases;
    int invalid_boundary_cases;
    int mismatch_count;
    int ok;
} GameWorldgenAridityDiminishingOasisHistogramSelfTest;

int game_worldgen_aridity_diminishing_oasis_pair_valid(
    int oasis_drop, int transition_margin);
void game_worldgen_aridity_diminishing_oasis_histogram_reset(
    GameWorldgenAridityDiminishingOasisHistogram *histogram);
int game_worldgen_aridity_diminishing_oasis_histogram_note_tile(
    GameWorldgenAridityDiminishingOasisHistogram *histogram,
    const struct WorldGenContext *context, int index, Climate climate);
int game_worldgen_aridity_diminishing_oasis_histogram_finalize(
    GameWorldgenAridityDiminishingOasisHistogram *histogram,
    const struct WorldGenContext *context);
int game_worldgen_aridity_diminishing_oasis_histogram_count(
    const GameWorldgenAridityDiminishingOasisHistogram *histogram,
    int oasis_drop, int transition_margin, int *count);
int game_worldgen_aridity_diminishing_oasis_histogram_crosscheck_legacy(
    GameWorldgenAridityDiminishingOasisHistogram *histogram,
    int oasis_drop, int transition_margin, int expected_count);
int game_worldgen_aridity_diminishing_oasis_histogram_self_test(
    GameWorldgenAridityDiminishingOasisHistogramSelfTest *result);

#endif
