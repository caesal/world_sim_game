#include "game/game_presentation_coast_smoothing_probe.h"

#include "render/render_water_coast_smoothing.h"

#include <string.h>

enum { FIXTURE_W = 48, FIXTURE_H = 32 };

static void add_rect(unsigned char *mask,
                     int x0, int y0, int x1, int y1) {
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) mask[y * FIXTURE_W + x] = 1;
}

static int run(const unsigned char *ocean, unsigned char *suppressed,
               RenderWaterCoastSmoothingMetrics *metrics) {
    return render_water_coast_smoothing_build(
        ocean, FIXTURE_W, FIXTURE_H, suppressed, metrics);
}

static int count_mask(const unsigned char *mask) {
    int count = 0;
    int i;
    for (i = 0; i < FIXTURE_W * FIXTURE_H; i++) count += mask[i] != 0;
    return count;
}

static int clustered_comb_contract(FILE *summary) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char first[FIXTURE_W * FIXTURE_H];
    unsigned char second[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics a;
    RenderWaterCoastSmoothingMetrics b;
    int row;
    int deterministic;
    int suppressed;
    int ok;
    add_rect(ocean, 0, 0, 11, FIXTURE_H - 1);
    for (row = 7; row <= 19; row += 4)
        add_rect(ocean, 12, row, 25, row);
    if (!run(ocean, first, &a) || !run(ocean, second, &b)) return 0;
    deterministic = memcmp(first, second, sizeof(first)) == 0 &&
                    memcmp(&a, &b, sizeof(a)) == 0;
    suppressed = count_mask(first);
    ok = deterministic && a.regularized_components == 4 &&
         a.regularized_tiles == 56 && suppressed == 56;
    fprintf(summary,
            "case=coast_smoothing_clustered_comb ok=%d deterministic=%d "
            "thin=%llu one_ended=%llu regularized=%llu tiles=%llu "
            "suppressed=%d\n",
            ok, deterministic, (unsigned long long)a.thin_components,
            (unsigned long long)a.one_ended_components,
            (unsigned long long)a.regularized_components,
            (unsigned long long)a.regularized_tiles, suppressed);
    return ok;
}

static int diagonal_contact_comb_contract(FILE *summary) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char first[FIXTURE_W * FIXTURE_H];
    unsigned char second[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics a;
    RenderWaterCoastSmoothingMetrics b;
    int stripe;
    int deterministic;
    int suppressed;
    int ok;
    for (stripe = 0; stripe < 3; stripe++) {
        int start_y = 3 + stripe * 6;
        int step;
        add_rect(ocean, 9, start_y - 3, 11, start_y - 1);
        add_rect(ocean, 20, start_y + 8, 22, start_y + 10);
        for (step = 0; step < 8; step++)
            ocean[(start_y + step) * FIXTURE_W + 12 + step] = 1;
    }
    if (!run(ocean, first, &a) || !run(ocean, second, &b)) return 0;
    deterministic = memcmp(first, second, sizeof(first)) == 0 &&
                    memcmp(&a, &b, sizeof(a)) == 0;
    suppressed = count_mask(first);
    ok = deterministic && a.thin_components >= 3 &&
         a.one_ended_components == 3 &&
         a.regularized_components == 3 && a.regularized_tiles == 24 &&
         suppressed == 24;
    fprintf(summary,
            "case=coast_smoothing_diagonal_contact_comb ok=%d "
            "deterministic=%d thin=%llu one_ended=%llu preserved=%llu "
            "regularized=%llu tiles=%llu suppressed=%d\n",
            ok, deterministic, (unsigned long long)a.thin_components,
            (unsigned long long)a.one_ended_components,
            (unsigned long long)a.preserved_components,
            (unsigned long long)a.regularized_components,
            (unsigned long long)a.regularized_tiles, suppressed);
    return ok;
}

static int clustered_passage_contract(FILE *summary) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics metrics;
    int row;
    int count;
    int ok;
    add_rect(ocean, 0, 0, 9, FIXTURE_H - 1);
    add_rect(ocean, 30, 0, FIXTURE_W - 1, FIXTURE_H - 1);
    for (row = 8; row <= 20; row += 4)
        add_rect(ocean, 10, row, 29, row);
    if (!run(ocean, suppressed, &metrics)) return 0;
    count = count_mask(suppressed);
    ok = metrics.preserved_components >= 4 &&
         metrics.regularized_components == 4 &&
         metrics.regularized_tiles == 80 && count == 80;
    fprintf(summary,
            "case=coast_smoothing_clustered_passages ok=%d "
            "preserved=%llu regularized=%llu tiles=%llu suppressed=%d\n",
            ok, (unsigned long long)metrics.preserved_components,
            (unsigned long long)metrics.regularized_components,
            (unsigned long long)metrics.regularized_tiles, count);
    return ok;
}

static int preservation_contract(FILE *summary) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H];
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics metrics;
    int isolated;
    int passage;
    int north_south_passage;
    int narrow_body;
    memset(ocean, 0, sizeof(ocean));
    add_rect(ocean, 0, 0, 11, FIXTURE_H - 1);
    add_rect(ocean, 12, 15, 29, 15);
    isolated = run(ocean, suppressed, &metrics) &&
               count_mask(suppressed) == 0;
    memset(ocean, 0, sizeof(ocean));
    add_rect(ocean, 0, 0, 9, FIXTURE_H - 1);
    add_rect(ocean, 34, 0, FIXTURE_W - 1, FIXTURE_H - 1);
    add_rect(ocean, 10, 15, 33, 15);
    passage = run(ocean, suppressed, &metrics) &&
              count_mask(suppressed) == 0 &&
              metrics.preserved_components > 0;
    memset(ocean, 0, sizeof(ocean));
    add_rect(ocean, 0, 0, FIXTURE_W - 1, 7);
    add_rect(ocean, 0, 24, FIXTURE_W - 1, FIXTURE_H - 1);
    add_rect(ocean, 24, 8, 24, 23);
    north_south_passage = run(ocean, suppressed, &metrics) &&
                          count_mask(suppressed) == 0 &&
                          metrics.preserved_components > 0;
    memset(ocean, 0, sizeof(ocean));
    add_rect(ocean, 20, 12, 21, 20);
    narrow_body = run(ocean, suppressed, &metrics) &&
                  count_mask(suppressed) == 0;
    fprintf(summary,
            "case=coast_smoothing_preservation ok=%d isolated=%d "
            "passage=%d north_south=%d narrow_body=%d constants=%d/%d/%d\n",
            isolated && passage && north_south_passage && narrow_body,
            isolated, passage, north_south_passage, narrow_body,
            WATER_COAST_SMOOTH_RADIUS,
            WATER_COAST_SMOOTH_CLUSTER_RADIUS,
            WATER_COAST_SMOOTH_MIN_CLUSTER);
    return isolated && passage && north_south_passage && narrow_body;
}

static int detached_shape_contract(FILE *summary) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char original[FIXTURE_W * FIXTURE_H];
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics slender_metrics;
    RenderWaterCoastSmoothingMetrics compact_metrics;
    int slender;
    int compact;
    int semantic_unchanged;
    add_rect(ocean, 10, 10, 10, 17);
    add_rect(ocean, 14, 10, 14, 17);
    add_rect(ocean, 18, 10, 18, 17);
    memcpy(original, ocean, sizeof(original));
    slender = run(ocean, suppressed, &slender_metrics) &&
              count_mask(suppressed) == 24 &&
              slender_metrics.regularized_components == 3 &&
              slender_metrics.candidate_comparisons > 0;
    semantic_unchanged = memcmp(ocean, original, sizeof(ocean)) == 0;
    slender &= semantic_unchanged;
    memset(ocean, 0, sizeof(ocean));
    add_rect(ocean, 10, 10, 11, 11);
    add_rect(ocean, 15, 10, 16, 11);
    add_rect(ocean, 20, 10, 21, 11);
    compact = run(ocean, suppressed, &compact_metrics) &&
              count_mask(suppressed) == 0 &&
              compact_metrics.preserved_components >= 3;
    fprintf(summary,
            "case=coast_texture_regularization_detached ok=%d slender=%d "
            "compact=%d semantic_mask_unchanged=%d comparisons=%llu "
            "compactness_only=1\n",
            slender && compact, slender, compact,
            semantic_unchanged,
            (unsigned long long)slender_metrics.candidate_comparisons);
    return slender && compact;
}

static int large_detached_contract(FILE *summary) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics clustered_metrics;
    RenderWaterCoastSmoothingMetrics isolated_metrics;
    int clustered;
    int isolated;
    add_rect(ocean, 6, 4, 41, 5);
    add_rect(ocean, 6, 10, 41, 11);
    add_rect(ocean, 6, 16, 41, 17);
    clustered = run(ocean, suppressed, &clustered_metrics) &&
                count_mask(suppressed) == 216 &&
                clustered_metrics.regularized_components == 3;
    memset(ocean, 0, sizeof(ocean));
    add_rect(ocean, 6, 10, 41, 11);
    isolated = run(ocean, suppressed, &isolated_metrics) &&
               count_mask(suppressed) == 0 &&
               isolated_metrics.regularized_components == 0;
    fprintf(summary,
            "case=coast_smoothing_large_detached ok=%d clustered=%d "
            "isolated=%d area=72x3 regularized=%llu/%llu\n",
            clustered && isolated, clustered, isolated,
            (unsigned long long)clustered_metrics.regularized_components,
            (unsigned long long)isolated_metrics.regularized_components);
    return clustered && isolated;
}

static int sparse_checker_contract(FILE *summary) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics metrics;
    int x;
    int y;
    int count;
    int ok;
    for (y = 8; y < 16; y++) {
        for (x = 12; x < 20; x++) {
            if (((x + y) & 1) == 0)
                ocean[y * FIXTURE_W + x] = 1;
        }
    }
    if (!run(ocean, suppressed, &metrics)) return 0;
    count = count_mask(suppressed);
    ok = count == 32 && metrics.thin_components == 1 &&
         metrics.sparse_mesh_components == 1 &&
         metrics.sparse_mesh_tiles == 32 &&
         metrics.regularized_components == 1;
    fprintf(summary,
            "case=coast_smoothing_sparse_checker ok=%d suppressed=%d "
            "thin=%llu sparse=%llu/%llu regularized=%llu\n",
            ok, count, (unsigned long long)metrics.thin_components,
            (unsigned long long)metrics.sparse_mesh_components,
            (unsigned long long)metrics.sparse_mesh_tiles,
            (unsigned long long)metrics.regularized_components);
    return ok;
}

int game_presentation_coast_smoothing_probe(FILE *summary) {
    int comb;
    int diagonal;
    int passages;
    int preservation;
    int detached;
    int large_detached;
    int sparse_checker;
    if (!summary) return 0;
    comb = clustered_comb_contract(summary);
    diagonal = diagonal_contact_comb_contract(summary);
    passages = clustered_passage_contract(summary);
    preservation = preservation_contract(summary);
    detached = detached_shape_contract(summary);
    large_detached = large_detached_contract(summary);
    sparse_checker = sparse_checker_contract(summary);
    fprintf(summary, "case=coast_smoothing_overall ok=%d\n",
            comb && diagonal && passages && preservation && detached &&
                large_detached && sparse_checker);
    return comb && diagonal && passages && preservation && detached &&
           large_detached && sparse_checker;
}
