#include "game/game_presentation_coast_multiscale_probe.h"

#include "render/render_water_coast_presentation.h"
#include "render/render_water_coast_smoothing.h"

#include <stdlib.h>
#include <string.h>

enum { FIXTURE_W = 96, FIXTURE_H = 80 };

static void add_rect(unsigned char *mask,
                     int x0, int y0, int x1, int y1) {
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) mask[y * FIXTURE_W + x] = 1;
}

static void add_diagonal(unsigned char *mask, int start_y,
                         int direction, int thickness) {
    int half = thickness / 2;
    int x;
    for (x = 16; x <= 58; x++) {
        int center_y = start_y + direction * (x - 16);
        int dx, dy;
        for (dy = -half; dy <= half; dy++) {
            for (dx = -half; dx <= half; dx++) {
                int px = x + dx;
                int py = center_y + dy;
                if (px >= 0 && py >= 0 && px < FIXTURE_W &&
                    py < FIXTURE_H)
                    mask[py * FIXTURE_W + px] = 1;
            }
        }
    }
}

static int run_smoothing(const unsigned char *ocean,
                         unsigned char *suppressed,
                         RenderWaterCoastSmoothingMetrics *metrics) {
    return render_water_coast_smoothing_build(
        ocean, FIXTURE_W, FIXTURE_H, suppressed, metrics);
}

static int count_mask(const unsigned char *mask, int count) {
    int total = 0;
    int i;
    for (i = 0; i < count; i++) total += mask[i] != 0;
    return total;
}

static int horizontal_width_contract(FILE *summary, int thickness) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char original[FIXTURE_W * FIXTURE_H];
    unsigned char first[FIXTURE_W * FIXTURE_H];
    unsigned char second[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics a, b;
    int starts[3] = {12, 12 + thickness + 2, 12 + 2 * (thickness + 2)};
    int x, y, band;
    int tail_failures = 0;
    int deterministic;
    int ok;
    add_rect(ocean, 0, 0, 15, FIXTURE_H - 1);
    for (band = 0; band < 3; band++)
        add_rect(ocean, 16, starts[band], 58,
                 starts[band] + thickness - 1);
    memcpy(original, ocean, sizeof(original));
    if (!run_smoothing(ocean, first, &a) ||
        !run_smoothing(ocean, second, &b)) return 0;
    for (band = 0; band < 3; band++)
        for (y = starts[band]; y < starts[band] + thickness; y++)
            for (x = 20; x <= 58; x++)
                tail_failures += !first[y * FIXTURE_W + x];
    deterministic = memcmp(first, second, sizeof(first)) == 0 &&
                    memcmp(&a, &b, sizeof(a)) == 0;
    ok = deterministic && tail_failures == 0 &&
         memcmp(ocean, original, sizeof(ocean)) == 0;
    fprintf(summary,
            "case=coast_multiscale_horizontal_%d ok=%d deterministic=%d "
            "tail_failures=%d suppressed=%d regularized=%llu/%llu\n",
            thickness, ok, deterministic, tail_failures,
            count_mask(first, FIXTURE_W * FIXTURE_H),
            (unsigned long long)a.regularized_components,
            (unsigned long long)a.regularized_tiles);
    return ok;
}

static int vertical_width_contract(FILE *summary, int thickness) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics metrics;
    int starts[3] = {12, 12 + thickness + 2, 12 + 2 * (thickness + 2)};
    int x, y, band;
    int tail_failures = 0;
    int ok;
    add_rect(ocean, 0, 0, FIXTURE_W - 1, 15);
    for (band = 0; band < 3; band++)
        add_rect(ocean, starts[band], 16,
                 starts[band] + thickness - 1, 58);
    if (!run_smoothing(ocean, suppressed, &metrics)) return 0;
    for (band = 0; band < 3; band++)
        for (x = starts[band]; x < starts[band] + thickness; x++)
            for (y = 20; y <= 58; y++)
                tail_failures += !suppressed[y * FIXTURE_W + x];
    ok = tail_failures == 0;
    fprintf(summary,
            "case=coast_multiscale_vertical_%d ok=%d tail_failures=%d "
            "suppressed=%d regularized=%llu/%llu\n",
            thickness, ok, tail_failures,
            count_mask(suppressed, FIXTURE_W * FIXTURE_H),
            (unsigned long long)metrics.regularized_components,
            (unsigned long long)metrics.regularized_tiles);
    return ok;
}

static int diagonal_contract(FILE *summary, int direction) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics metrics;
    int starts[3];
    int sample_failures = 0;
    int band, x;
    int ok;
    starts[0] = direction > 0 ? 5 : 74;
    starts[1] = starts[0] + direction * 8;
    starts[2] = starts[1] + direction * 8;
    add_rect(ocean, 0, 0, 15, FIXTURE_H - 1);
    for (band = 0; band < 3; band++)
        add_diagonal(ocean, starts[band], direction, 3);
    if (!run_smoothing(ocean, suppressed, &metrics)) return 0;
    for (band = 0; band < 3; band++) {
        for (x = 24; x <= 50; x++) {
            int y = starts[band] + direction * (x - 16);
            sample_failures += !suppressed[y * FIXTURE_W + x];
        }
    }
    ok = sample_failures == 0;
    fprintf(summary,
            "case=coast_multiscale_diagonal_%s ok=%d sample_failures=%d "
            "suppressed=%d regularized=%llu/%llu\n",
            direction > 0 ? "down" : "up", ok, sample_failures,
            count_mask(suppressed, FIXTURE_W * FIXTURE_H),
            (unsigned long long)metrics.regularized_components,
            (unsigned long long)metrics.regularized_tiles);
    return ok;
}

static int connected_comb_contract(FILE *summary, int orientation) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics metrics;
    int failures = 0;
    int x, y, band;
    int ok;
    if (orientation == 0) {
        static const int starts[3] = {12, 20, 28};
        add_rect(ocean, 0, 0, 15, FIXTURE_H - 1);
        for (band = 0; band < 3; band++)
            add_rect(ocean, 16, starts[band], 58, starts[band] + 2);
        add_rect(ocean, 56, 12, 58, 30);
        if (!run_smoothing(ocean, suppressed, &metrics)) return 0;
        for (band = 0; band < 3; band++)
            for (y = starts[band]; y <= starts[band] + 2; y++)
                for (x = 20; x <= 58; x++)
                    failures += !suppressed[y * FIXTURE_W + x];
    } else if (orientation == 1) {
        static const int starts[3] = {12, 20, 28};
        add_rect(ocean, 0, 0, FIXTURE_W - 1, 15);
        for (band = 0; band < 3; band++)
            add_rect(ocean, starts[band], 16, starts[band] + 2, 58);
        add_rect(ocean, 12, 56, 30, 58);
        if (!run_smoothing(ocean, suppressed, &metrics)) return 0;
        for (band = 0; band < 3; band++)
            for (x = starts[band]; x <= starts[band] + 2; x++)
                for (y = 20; y <= 58; y++)
                    failures += !suppressed[y * FIXTURE_W + x];
    } else {
        int direction = orientation == 2 ? 1 : -1;
        int starts[3];
        int min_y = direction > 0 ? 45 : 15;
        int max_y = direction > 0 ? 65 : 35;
        starts[0] = direction > 0 ? 5 : 74;
        starts[1] = starts[0] + direction * 8;
        starts[2] = starts[1] + direction * 8;
        add_rect(ocean, 0, 0, 15, FIXTURE_H - 1);
        for (band = 0; band < 3; band++)
            add_diagonal(ocean, starts[band], direction, 3);
        add_rect(ocean, 56, min_y, 58, max_y);
        if (!run_smoothing(ocean, suppressed, &metrics)) return 0;
        for (band = 0; band < 3; band++) {
            for (x = 24; x <= 50; x++) {
                y = starts[band] + direction * (x - 16);
                failures += !suppressed[y * FIXTURE_W + x];
            }
        }
    }
    ok = failures == 0 && metrics.sparse_mesh_components > 0;
    fprintf(summary,
            "case=coast_multiscale_connected_%d ok=%d failures=%d "
            "sparse=%llu/%llu suppressed=%d\n",
            orientation, ok, failures,
            (unsigned long long)metrics.sparse_mesh_components,
            (unsigned long long)metrics.sparse_mesh_tiles,
            count_mask(suppressed, FIXTURE_W * FIXTURE_H));
    return ok;
}

static int legitimate_water_contract(FILE *summary) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H] = {0};
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics metrics;
    int isolated;
    int passage;
    add_rect(ocean, 0, 0, 15, FIXTURE_H - 1);
    add_rect(ocean, 16, 34, 58, 40);
    isolated = run_smoothing(ocean, suppressed, &metrics) &&
               count_mask(suppressed, FIXTURE_W * FIXTURE_H) == 0;
    memset(ocean, 0, sizeof(ocean));
    add_rect(ocean, 0, 0, 15, FIXTURE_H - 1);
    add_rect(ocean, 64, 0, FIXTURE_W - 1, FIXTURE_H - 1);
    add_rect(ocean, 16, 34, 63, 40);
    passage = run_smoothing(ocean, suppressed, &metrics) &&
              count_mask(suppressed, FIXTURE_W * FIXTURE_H) == 0;
    fprintf(summary,
            "case=coast_multiscale_legitimate_water ok=%d "
            "isolated_bay=%d single_passage=%d\n",
            isolated && passage, isolated, passage);
    return isolated && passage;
}

static int complex_legitimate_water_contract(FILE *summary) {
    unsigned char ocean[FIXTURE_W * FIXTURE_H];
    unsigned char suppressed[FIXTURE_W * FIXTURE_H];
    RenderWaterCoastSmoothingMetrics metrics;
    int diagonal_widths = 1;
    int u_bay;
    int diagonal_passage;
    int thickness;
    for (thickness = 3; thickness <= 7; thickness += 2) {
        int half = thickness / 2;
        int x;
        memset(ocean, 0, sizeof(ocean));
        add_rect(ocean, 0, 0, 15, FIXTURE_H - 1);
        for (x = 16; x <= 62; x++) {
            int center_y = 14 + (x - 16) / 2;
            int dx, dy;
            for (dy = -half; dy <= half; dy++)
                for (dx = -half; dx <= half; dx++)
                    ocean[(center_y + dy) * FIXTURE_W + x + dx] = 1;
        }
        diagonal_widths &= run_smoothing(ocean, suppressed, &metrics) &&
                           count_mask(suppressed,
                                      FIXTURE_W * FIXTURE_H) == 0 &&
                           metrics.converged;
    }
    memset(ocean, 0, sizeof(ocean));
    add_rect(ocean, 0, 0, 15, FIXTURE_H - 1);
    add_rect(ocean, 16, 18, 62, 20);
    add_rect(ocean, 16, 42, 62, 44);
    add_rect(ocean, 60, 18, 62, 44);
    u_bay = run_smoothing(ocean, suppressed, &metrics) &&
            count_mask(suppressed, FIXTURE_W * FIXTURE_H) == 0 &&
            metrics.converged;
    memset(ocean, 0, sizeof(ocean));
    add_rect(ocean, 0, 0, 15, FIXTURE_H - 1);
    add_rect(ocean, 72, 0, FIXTURE_W - 1, FIXTURE_H - 1);
    for (thickness = 16; thickness <= 72; thickness++) {
        int center_y = 18 + (thickness - 16) / 3;
        add_rect(ocean, thickness, center_y - 1,
                 thickness, center_y + 1);
    }
    diagonal_passage = run_smoothing(ocean, suppressed, &metrics) &&
                       count_mask(suppressed,
                                  FIXTURE_W * FIXTURE_H) == 0 &&
                       metrics.converged;
    fprintf(summary,
            "case=coast_multiscale_complex_legitimate ok=%d "
            "diagonal_widths=%d u_bay=%d diagonal_passage=%d\n",
            diagonal_widths && u_bay && diagonal_passage,
            diagonal_widths, u_bay, diagonal_passage);
    return diagonal_widths && u_bay && diagonal_passage;
}

static int bounded_anchor_connector_contract(FILE *summary) {
    enum { W = 64, H = 40 };
    unsigned char ocean[W * H] = {0};
    unsigned char protected_ocean[W * H] = {0};
    unsigned char suppressed[W * H] = {0};
    uint64_t components = 0, tiles = 0, bytes = 0;
    int x, y;
    int restored = 0;
    int branch_restored = 0;
    int ok;
    for (y = 0; y < H; y++)
        for (x = 0; x <= 5; x++) ocean[y * W + x] = 1;
    for (y = 9; y <= 11; y++) {
        for (x = 6; x <= 35; x++) {
            ocean[y * W + x] = 1;
            suppressed[y * W + x] = 1;
        }
    }
    for (y = 12; y <= 24; y++) {
        ocean[y * W + 20] = 1;
        ocean[y * W + 28] = 1;
        suppressed[y * W + 20] = 1;
        suppressed[y * W + 28] = 1;
    }
    protected_ocean[10 * W + 35] = 1;
    ok = render_water_coast_presentation_preserve_anchored_components(
        ocean, protected_ocean, W, H, suppressed,
        &components, &tiles, &bytes);
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            if (ocean[y * W + x] && x > 5 && !suppressed[y * W + x])
                restored++;
        }
    }
    for (y = 12; y <= 24; y++)
        branch_restored += !suppressed[y * W + 20] ||
                           !suppressed[y * W + 28];
    ok &= components == 1 && tiles == 30 && restored == 30 &&
          branch_restored == 0;
    fprintf(summary,
            "case=coast_multiscale_bounded_anchor ok=%d components=%llu "
            "tiles=%llu restored=%d branch_restored=%d transient=%llu\n",
            ok, (unsigned long long)components,
            (unsigned long long)tiles, restored, branch_restored,
            (unsigned long long)bytes);
    return ok;
}

static int natural_residual_contract(FILE *summary,
                                     const RenderSnapshot *snapshot) {
    RenderWaterCoastPresentationMetrics coast = {0};
    RenderWaterCoastSmoothingMetrics residual_metrics = {0};
    unsigned char *categories = NULL;
    unsigned char *ocean = NULL;
    unsigned char *residual = NULL;
    unsigned char *protected_ocean = NULL;
    size_t count;
    int residual_tiles = -1;
    int unprotected_residual_tiles = -1;
    uint64_t marine_tiles = 0;
    uint64_t protected_components = 0;
    uint64_t protected_tiles = 0;
    uint64_t protection_bytes = 0;
    int i;
    int ok = 0;
    if (!snapshot || !snapshot->world_generated || snapshot->map_w <= 0 ||
        snapshot->map_h <= 0) return 0;
    count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    categories = (unsigned char *)malloc(count);
    ocean = (unsigned char *)calloc(count, 1);
    residual = (unsigned char *)malloc(count);
    protected_ocean = (unsigned char *)malloc(count);
    if (!categories || !ocean || !residual || !protected_ocean) goto cleanup;
    if (!render_water_coast_presentation_build(snapshot, categories, &coast))
        goto cleanup;
    for (i = 0; i < (int)count; i++)
        ocean[i] = categories[i] == WATER_COAST_PRESENTATION_OCEAN;
    if (!render_water_coast_smoothing_build(
            ocean, snapshot->map_w, snapshot->map_h,
            residual, &residual_metrics)) goto cleanup;
    residual_tiles = count_mask(residual, (int)count);
    if (!render_water_coast_presentation_build_marine_protection(
            snapshot, protected_ocean, &marine_tiles) ||
        !render_water_coast_presentation_preserve_anchored_components(
            ocean, protected_ocean, snapshot->map_w, snapshot->map_h,
            residual, &protected_components, &protected_tiles,
            &protection_bytes)) goto cleanup;
    unprotected_residual_tiles = count_mask(residual, (int)count);
    ok = unprotected_residual_tiles == 0;
cleanup:
    fprintf(summary,
            "case=coast_multiscale_natural_residual ok=%d revision=%llu "
            "removed=%llu residual=%d unprotected=%d "
            "residual_components=%llu anchors=%llu/%llu\n",
            ok, snapshot ? (unsigned long long)snapshot->revision : 0,
            (unsigned long long)coast.removed_ocean_tiles,
            residual_tiles, unprotected_residual_tiles,
            (unsigned long long)residual_metrics.regularized_components,
            (unsigned long long)protected_components,
            (unsigned long long)protected_tiles);
    free(protected_ocean);
    free(residual);
    free(ocean);
    free(categories);
    return ok;
}

int game_presentation_coast_multiscale_synthetic_probe(FILE *summary) {
    int ok = 1;
    if (!summary) return 0;
    ok &= horizontal_width_contract(summary, 3);
    ok &= horizontal_width_contract(summary, 5);
    ok &= horizontal_width_contract(summary, 7);
    ok &= vertical_width_contract(summary, 3);
    ok &= vertical_width_contract(summary, 5);
    ok &= vertical_width_contract(summary, 7);
    ok &= diagonal_contract(summary, 1);
    ok &= diagonal_contract(summary, -1);
    ok &= connected_comb_contract(summary, 0);
    ok &= connected_comb_contract(summary, 1);
    ok &= connected_comb_contract(summary, 2);
    ok &= connected_comb_contract(summary, 3);
    ok &= legitimate_water_contract(summary);
    ok &= complex_legitimate_water_contract(summary);
    ok &= bounded_anchor_connector_contract(summary);
    return ok;
}

int game_presentation_coast_multiscale_probe(
    FILE *summary, const RenderSnapshot *natural_snapshot) {
    int ok;
    if (!summary || !natural_snapshot) return 0;
    ok = game_presentation_coast_multiscale_synthetic_probe(summary);
    ok &= natural_residual_contract(summary, natural_snapshot);
    fprintf(summary, "case=coast_multiscale_overall ok=%d\n", ok);
    return ok;
}
