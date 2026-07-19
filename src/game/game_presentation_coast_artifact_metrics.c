#include "game/game_presentation_coast_artifact_metrics.h"

#include "game/game_presentation_coast_artifact_pattern.h"

#include "core/constants.h"
#include "render/render_ocean_coverage.h"
#include "render/render_water_coast_presentation.h"
#include "render/render_water_coverage.h"
#include "ui/ui_layout.h"

#include <stddef.h>
#include <string.h>

typedef enum {
    PALETTE_NONE,
    PALETTE_CYAN,
    PALETTE_GREEN
} RejectedPaletteClass;

typedef struct {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char kind;
} RejectedPaletteSample;

/* Exact colors sampled from the rejected cyan/green comb screenshot.  The
   tolerance catches its one- and two-level GDI resampling variants. */
static const RejectedPaletteSample rejected_palette[] = {
    {70, 145, 190, PALETTE_CYAN}, {79, 172, 201, PALETTE_CYAN},
    {83, 174, 206, PALETTE_CYAN}, {110, 159, 141, PALETTE_CYAN},
    {115, 167, 170, PALETTE_CYAN}, {150, 196, 170, PALETTE_CYAN},
    {169, 203, 123, PALETTE_GREEN}, {168, 202, 122, PALETTE_GREEN},
    {138, 186, 90, PALETTE_GREEN}, {149, 192, 111, PALETTE_GREEN},
    {158, 197, 101, PALETTE_GREEN}, {183, 225, 170, PALETTE_GREEN},
    {96, 144, 105, PALETTE_GREEN}, {97, 145, 106, PALETTE_GREEN}
};

static uint32_t packed_rgb(unsigned int red, unsigned int green,
                           unsigned int blue) {
    return (red << 16) | (green << 8) | blue;
}

static int channel_near(unsigned int left, unsigned int right) {
    int delta = (int)left - (int)right;
    if (delta < 0) delta = -delta;
    return delta <= COAST_ARTIFACT_REJECT_TOLERANCE;
}

static RejectedPaletteClass rejected_class(uint32_t color, int *exact) {
    unsigned int red = (color >> 16) & 255u;
    unsigned int green = (color >> 8) & 255u;
    unsigned int blue = color & 255u;
    size_t i;
    if (exact) *exact = 0;
    for (i = 0; i < sizeof(rejected_palette) / sizeof(rejected_palette[0]); i++) {
        const RejectedPaletteSample *sample = &rejected_palette[i];
        if (!channel_near(red, sample->red) ||
            !channel_near(green, sample->green) ||
            !channel_near(blue, sample->blue)) continue;
        if (exact)
            *exact = red == sample->red && green == sample->green &&
                     blue == sample->blue;
        return (RejectedPaletteClass)sample->kind;
    }
    return PALETTE_NONE;
}

static RejectedPaletteClass broad_family(uint32_t color) {
    unsigned int red = (color >> 16) & 255u;
    unsigned int green = (color >> 8) & 255u;
    unsigned int blue = color & 255u;
    if (blue >= 145u && green >= 120u && blue >= red + 25u &&
        green >= red + 18u) return PALETTE_CYAN;
    if (green >= 140u && green >= red + 20u && green >= blue + 20u)
        return PALETTE_GREEN;
    return PALETTE_NONE;
}

static int nearest_tile_at_pixel(const RenderSnapshot *snapshot,
                                 MapLayout layout, int px, int py,
                                 int *x, int *y) {
    long long local_x;
    long long local_y;
    if (px < layout.map_x || py < layout.map_y ||
        px >= layout.map_x + layout.draw_w ||
        py >= layout.map_y + layout.draw_h) return 0;
    local_x = (long long)(px - layout.map_x) * 2 + 1;
    local_y = (long long)(py - layout.map_y) * 2 + 1;
    *x = (int)(local_x * snapshot->map_w / (2LL * layout.draw_w));
    *y = (int)(local_y * snapshot->map_h / (2LL * layout.draw_h));
    return *x >= 0 && *y >= 0 && *x < snapshot->map_w &&
           *y < snapshot->map_h;
}

static int ocean_geography(int geography) {
    return geography == GEO_OCEAN || geography == GEO_BAY;
}

static int land_geography(int geography) {
    return !ocean_geography(geography) && geography != GEO_LAKE;
}

static int presentation_category(
    const RenderSnapshot *snapshot, const unsigned char *categories,
    int x, int y) {
    int geography = snapshot->tiles[y * snapshot->map_w + x].geography;
    if (categories) return categories[y * snapshot->map_w + x];
    if (geography == GEO_LAKE) return WATER_COAST_PRESENTATION_LAKE;
    return ocean_geography(geography) ? WATER_COAST_PRESENTATION_OCEAN :
                                       WATER_COAST_PRESENTATION_LAND;
}

static int presentation_ocean(
    const RenderSnapshot *snapshot, const unsigned char *categories,
    int x, int y) {
    return presentation_category(snapshot, categories, x, y) ==
           WATER_COAST_PRESENTATION_OCEAN;
}

static int presentation_land(
    const RenderSnapshot *snapshot, const unsigned char *categories,
    int x, int y) {
    return presentation_category(snapshot, categories, x, y) ==
           WATER_COAST_PRESENTATION_LAND;
}

static int green_coast_geography(int geography) {
    return geography == GEO_WETLAND || geography == GEO_OASIS ||
           geography == GEO_ISLAND || geography == GEO_DELTA;
}

static int near_water_boundary(
    const RenderSnapshot *snapshot, const unsigned char *categories,
    int x, int y, int include_lake) {
    int water = 0;
    int land = 0;
    int dx, dy;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            int geography;
            if (nx < 0 || ny < 0 || nx >= snapshot->map_w ||
                ny >= snapshot->map_h) continue;
            geography = snapshot->tiles[ny * snapshot->map_w + nx].geography;
            if (presentation_ocean(snapshot, categories, nx, ny) ||
                (include_lake && geography == GEO_LAKE)) water = 1;
            else if (presentation_land(snapshot, categories, nx, ny)) land = 1;
        }
    }
    return water && land;
}

static int near_stage_water(
    const RenderSnapshot *snapshot, const unsigned char *categories,
    int x, int y, CoastArtifactIsolationLayer layer) {
    int dx, dy;
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            int geography;
            if (nx < 0 || ny < 0 || nx >= snapshot->map_w ||
                ny >= snapshot->map_h) continue;
            geography = snapshot->tiles[
                ny * snapshot->map_w + nx].geography;
            if (layer == COAST_ARTIFACT_LAYER_LAKE &&
                geography == GEO_LAKE) return 1;
            if (layer == COAST_ARTIFACT_LAYER_OCEAN &&
                presentation_ocean(snapshot, categories, nx, ny)) return 1;
        }
    }
    return 0;
}

static void build_geometry_leak_mask(
    const StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot, const unsigned char *categories,
    MapLayout layout,
    CoastArtifactIsolationLayer layer, const uint32_t *previous,
    RECT viewport, unsigned char *mask,
    CoastArtifactIsolationMetrics *metrics) {
    int x, y;
    memset(mask, 0, (size_t)canvas->width * (size_t)canvas->height);
    if (!previous || (layer != COAST_ARTIFACT_LAYER_LAKE &&
                      layer != COAST_ARTIFACT_LAYER_OCEAN)) return;
    for (y = viewport.top; y < viewport.bottom; y++) {
        for (x = viewport.left; x < viewport.right; x++) {
            int index = y * canvas->width + x;
            int tile_x, tile_y;
            int geography;
            uint32_t color = canvas->pixels[index] & UINT32_C(0x00ffffff);
            if (color == (previous[index] & UINT32_C(0x00ffffff)) ||
                !nearest_tile_at_pixel(snapshot, layout, x, y,
                                       &tile_x, &tile_y))
                continue;
            geography = snapshot->tiles[
                tile_y * snapshot->map_w + tile_x].geography;
            if (layer == COAST_ARTIFACT_LAYER_LAKE &&
                !land_geography(geography)) continue;
            if (layer == COAST_ARTIFACT_LAYER_OCEAN &&
                !presentation_land(snapshot, categories,
                                   tile_x, tile_y)) continue;
            if (!near_stage_water(snapshot, categories,
                                  tile_x, tile_y, layer)) continue;
            mask[index] = 1;
            metrics->geometry_leak_pixels++;
        }
    }
    game_presentation_coast_pattern_detect(
        mask, canvas->width, canvas->height,
        &metrics->geometry_thin_runs, &metrics->geometry_comb_clusters);
}

static int layer_candidate(CoastArtifactIsolationLayer layer,
                           const RenderSnapshot *snapshot,
                           const unsigned char *categories,
                           int tile_x, int tile_y, int changed) {
    if (layer == COAST_ARTIFACT_LAYER_BASE)
        return presentation_land(snapshot, categories, tile_x, tile_y);
    if (layer == COAST_ARTIFACT_LAYER_COAST)
        return changed && near_water_boundary(
            snapshot, categories, tile_x, tile_y, 0);
    if (layer == COAST_ARTIFACT_LAYER_LAKE)
        return changed && near_water_boundary(
            snapshot, categories, tile_x, tile_y, 1);
    if (layer == COAST_ARTIFACT_LAYER_OCEAN)
        return changed && near_water_boundary(
            snapshot, categories, tile_x, tile_y, 0);
    return changed;
}

int game_presentation_coast_artifact_analyze(
    const StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot, const unsigned char *categories,
    MapLayout layout,
    CoastArtifactIsolationLayer layer, const uint32_t *previous,
    unsigned char *mask, CoastArtifactIsolationMetrics *metrics) {
    RECT client = {0, 0, canvas->width, canvas->height};
    RECT viewport = get_map_content_rect(client);
    int x, y;
    memset(metrics, 0, sizeof(*metrics));
    memset(mask, 0, (size_t)canvas->width * (size_t)canvas->height);
    for (y = viewport.top; y < viewport.bottom; y++) {
        for (x = viewport.left; x < viewport.right; x++) {
            int index = y * canvas->width + x;
            uint32_t color = canvas->pixels[index] & UINT32_C(0x00ffffff);
            int changed = !previous ||
                          color != (previous[index] & UINT32_C(0x00ffffff));
            int exact = 0;
            RejectedPaletteClass rejected;
            RejectedPaletteClass family;
            int tile_x, tile_y;
            int geography;
            int broad_candidate;
            if (!changed || !nearest_tile_at_pixel(snapshot, layout, x, y,
                                                   &tile_x, &tile_y)) continue;
            metrics->changed_pixels++;
            geography = snapshot->tiles[
                tile_y * snapshot->map_w + tile_x].geography;
            rejected = rejected_class(color, &exact);
            family = broad_family(color);
            metrics->family_cyan_pixels += family == PALETTE_CYAN;
            metrics->family_green_pixels += family == PALETTE_GREEN;
            metrics->family_island_pixels +=
                family == PALETTE_GREEN && geography == GEO_ISLAND;
            metrics->family_wetland_pixels +=
                family == PALETTE_GREEN && geography == GEO_WETLAND;
            /* Wetland, oasis, island, and delta colors are semantic land
               presentation, not leakage. Overlay stages are still checked
               through their changed-pixel and geometry masks below. */
            if (layer == COAST_ARTIFACT_LAYER_BASE &&
                family == PALETTE_GREEN &&
                green_coast_geography(geography)) continue;
            broad_candidate = family != PALETTE_NONE &&
                ((layer == COAST_ARTIFACT_LAYER_BASE &&
                  family == PALETTE_CYAN && presentation_land(
                      snapshot, categories, tile_x, tile_y)) ||
                 (layer == COAST_ARTIFACT_LAYER_COAST &&
                  near_water_boundary(snapshot, categories,
                                      tile_x, tile_y, 0)) ||
                 (layer == COAST_ARTIFACT_LAYER_LAKE &&
                  land_geography(geography) &&
                  near_water_boundary(snapshot, categories,
                                      tile_x, tile_y, 1)) ||
                 (layer == COAST_ARTIFACT_LAYER_OCEAN &&
                  presentation_land(snapshot, categories, tile_x, tile_y) &&
                  near_water_boundary(snapshot, categories,
                                      tile_x, tile_y, 0)));
            if (rejected == PALETTE_NONE && !broad_candidate) continue;
            metrics->near_cyan_pixels += rejected == PALETTE_CYAN;
            metrics->near_green_pixels += rejected == PALETTE_GREEN;
            metrics->exact_cyan_pixels += exact && rejected == PALETTE_CYAN;
            metrics->exact_green_pixels += exact && rejected == PALETTE_GREEN;
            if (!layer_candidate(layer, snapshot, categories,
                                 tile_x, tile_y, changed))
                continue;
            mask[index] = 1;
            metrics->candidate_pixels++;
            metrics->candidate_ocean_pixels += presentation_ocean(
                snapshot, categories, tile_x, tile_y);
            metrics->candidate_lake_pixels += geography == GEO_LAKE;
            metrics->candidate_land_pixels += presentation_land(
                snapshot, categories, tile_x, tile_y);
            metrics->coast_mask_leak_pixels +=
                layer == COAST_ARTIFACT_LAYER_COAST &&
                ocean_geography(geography);
            metrics->water_land_leak_pixels +=
                (layer == COAST_ARTIFACT_LAYER_LAKE &&
                 land_geography(geography)) ||
                (layer == COAST_ARTIFACT_LAYER_OCEAN &&
                 presentation_land(snapshot, categories, tile_x, tile_y));
        }
    }
    game_presentation_coast_pattern_detect(
        mask, canvas->width, canvas->height,
        &metrics->palette_thin_runs, &metrics->palette_comb_clusters);
    build_geometry_leak_mask(canvas, snapshot, categories, layout, layer,
                             previous, viewport, mask, metrics);
    metrics->thin_runs = metrics->palette_thin_runs +
                         metrics->geometry_thin_runs;
    metrics->comb_clusters = metrics->palette_comb_clusters +
                             metrics->geometry_comb_clusters;
    return metrics->comb_clusters == 0;
}

int game_presentation_coast_artifact_detector_contract(
    int *comb_clusters, int *single_clusters) {
    enum { W = 64, H = 48 };
    unsigned char comb[W * H] = {0};
    unsigned char single[W * H] = {0};
    unsigned char fringe[W * H] = {0};
    unsigned char broad_stage[W * H] = {0};
    unsigned char broad_land_leak[W * H] = {0};
    int thin_runs;
    int single_runs;
    int broad_runs;
    int broad_clusters;
    int fringe_runs;
    int fringe_clusters;
    int exact_cyan = 0;
    int exact_green = 0;
    int stripe, i;
    RejectedPaletteClass cyan = rejected_class(
        packed_rgb(70, 145, 190), &exact_cyan);
    RejectedPaletteClass green = rejected_class(
        packed_rgb(169, 203, 123), &exact_green);
    for (stripe = 0; stripe < 4; stripe++) {
        for (i = 0; i < 10; i++) {
            int x = 20 + i - stripe * 2;
            int y = 4 + i + stripe * 2;
            comb[y * W + x] = 1;
        }
    }
    for (i = 0; i < 20; i++) single[(4 + i) * W + 4 + i] = 1;
    for (i = 4; i < H - 4; i++) {
        fringe[i * W + 20] = 1;
        fringe[i * W + 21] = 1;
    }
    for (i = 0; i < W * H; i++) {
        int x = i % W;
        if (x < 20) broad_stage[i] = 1;
    }
    for (stripe = 0; stripe < 4; stripe++) {
        int y = 10 + stripe * 4;
        for (i = 20; i < 38; i++) broad_stage[y * W + i] = 1;
    }
    for (i = 0; i < W * H; i++)
        if (i % W >= 20) broad_land_leak[i] = broad_stage[i];
    game_presentation_coast_pattern_detect(
        comb, W, H, &thin_runs, comb_clusters);
    game_presentation_coast_pattern_detect(
        single, W, H, &single_runs, single_clusters);
    game_presentation_coast_pattern_detect(
        fringe, W, H, &fringe_runs, &fringe_clusters);
    game_presentation_coast_pattern_detect(
        broad_land_leak, W, H, &broad_runs, &broad_clusters);
    return cyan == PALETTE_CYAN && green == PALETTE_GREEN &&
           exact_cyan && exact_green && thin_runs >= 3 && single_runs > 0 &&
           broad_runs >= 3 && broad_clusters > 0 &&
           fringe_runs == 0 && fringe_clusters == 0 &&
           *comb_clusters > 0 && *single_clusters == 0;
}

static int water_code(const RenderSnapshot *snapshot, int x, int y,
                      int lake) {
    static const int dx[4] = {0, 1, 1, 0};
    static const int dy[4] = {0, 0, 1, 1};
    int code = 0;
    int corner;
    for (corner = 0; corner < 4; corner++) {
        int geography = snapshot->tiles[
            (y + dy[corner]) * snapshot->map_w + x + dx[corner]].geography;
        int water = lake ? geography == GEO_LAKE : ocean_geography(geography);
        code |= water << corner;
    }
    return code;
}

static int diagonal_checker(const int samples[4]) {
    return samples[0] == samples[2] && samples[1] == samples[3] &&
           samples[0] != samples[1];
}

static int fractional_cell(int x, int y, int scale, int lake) {
    int base_x = x * scale + scale / 2;
    int base_y = y * scale + scale / 2;
    int dx, dy;
    for (dy = 0; dy < scale; dy++) {
        for (dx = 0; dx < scale; dx++) {
            int alpha = lake ? render_water_coverage_lake_alpha(
                                   base_x + dx, base_y + dy) :
                               render_water_coverage_ocean_alpha(
                                   base_x + dx, base_y + dy);
            if (alpha > 0 && alpha < 255) return 1;
        }
    }
    return 0;
}

int game_presentation_coast_artifact_saddle_contract(
    const RenderSnapshot *snapshot, CoastArtifactSaddleMetrics *metrics) {
    int scale = render_water_coverage_scale();
    int x, y;
    if (!snapshot || !metrics || !render_ocean_coverage_prepare_field(snapshot))
        return 0;
    memset(metrics, 0, sizeof(*metrics));
    for (y = 0; y + 1 < snapshot->map_h; y++) {
        for (x = 0; x + 1 < snapshot->map_w; x++) {
            int ocean_code = water_code(snapshot, x, y, 0);
            int lake_code = water_code(snapshot, x, y, 1);
            int ocean[4], lake[4];
            int ocean_hard[4], lake_hard[4];
            int base_x = x * scale + scale / 2;
            int base_y = y * scale + scale / 2;
            int lo = max(0, scale / 4);
            int hi = min(scale - 1, scale * 3 / 4);
            int px[4] = {base_x + lo, base_x + hi,
                         base_x + hi, base_x + lo};
            int py[4] = {base_y + lo, base_y + lo,
                         base_y + hi, base_y + hi};
            int corner;
            if (ocean_code != 5 && ocean_code != 10 &&
                lake_code != 5 && lake_code != 10) continue;
            for (corner = 0; corner < 4; corner++) {
                ocean[corner] = render_water_coverage_ocean_alpha(
                    px[corner], py[corner]);
                lake[corner] = render_water_coverage_lake_alpha(
                    px[corner], py[corner]);
                ocean_hard[corner] = ocean[corner] >= 128;
                lake_hard[corner] = lake[corner] >= 128;
            }
            if ((ocean_code == 5 || ocean_code == 10) && lake_code == 0) {
                int fractional = fractional_cell(x, y, scale, 0);
                metrics->ocean_ambiguous_cells++;
                metrics->ocean_fractional_cells += fractional;
                metrics->ocean_checker_cells +=
                    diagonal_checker(ocean_hard) &&
                    !fractional;
            }
            if ((lake_code == 5 || lake_code == 10) && ocean_code == 0) {
                int fractional = fractional_cell(x, y, scale, 1);
                metrics->lake_ambiguous_cells++;
                metrics->lake_fractional_cells += fractional;
                metrics->lake_checker_cells +=
                    diagonal_checker(lake_hard) && !fractional;
            }
        }
    }
    return metrics->ocean_checker_cells == 0 &&
           metrics->lake_checker_cells == 0 &&
           (metrics->ocean_ambiguous_cells > 0 ?
                metrics->ocean_fractional_cells > 0 :
                metrics->ocean_fractional_cells == 0) &&
           (metrics->lake_ambiguous_cells > 0 ?
                metrics->lake_fractional_cells > 0 :
                metrics->lake_fractional_cells == 0);
}
