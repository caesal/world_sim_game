#include "game/game_presentation_coast_artifact_probe.h"

#include "core/constants.h"
#include "game/game_presentation_coast_artifact_metrics.h"
#include "game/game_presentation_water_edge_probe.h"
#include "render/coast_geometry.h"
#include "render/map_display_policy.h"
#include "render/map_shore_color_cache.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_static_map_surface.h"
#include "render/render_water_coast_presentation.h"
#include "render/render_water_coverage.h"
#include "render/render_water_surface_cache.h"
#include "render/river_render.h"
#include "ui/ui_layout.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { CANONICAL_SCALE = 2 };

static MapLayout centered_layout(const RenderSnapshot *snapshot,
                                 RECT client, int zoom) {
    RECT viewport = get_map_content_rect(client);
    int available_w = viewport.right - viewport.left;
    int available_h = viewport.bottom - viewport.top;
    int fit_w = available_w;
    int fit_h = fit_w * snapshot->map_h / snapshot->map_w;
    MapLayout layout;
    if (fit_h > available_h) {
        fit_h = available_h;
        fit_w = fit_h * snapshot->map_w / snapshot->map_h;
    }
    layout.draw_w = fit_w * zoom / 100;
    layout.draw_h = fit_h * zoom / 100;
    layout.tile_size = layout.draw_w / snapshot->map_w;
    if (layout.tile_size < 1) layout.tile_size = 1;
    layout.map_x = viewport.left + (available_w - layout.draw_w) / 2;
    layout.map_y = viewport.top + (available_h - layout.draw_h) / 2;
    return layout;
}

static void present_canonical(StaticPhysicalProbeCanvas *destination,
                              const StaticPhysicalProbeCanvas *source,
                              RECT client, MapLayout layout) {
    RECT viewport = get_map_content_rect(client);
    int stretch_mode = render_static_map_surface_categorical_stretch_mode();
    int saved = SaveDC(destination->dc);
    IntersectClipRect(destination->dc, viewport.left, viewport.top,
                      viewport.right, viewport.bottom);
    SetStretchBltMode(destination->dc, stretch_mode);
    StretchBlt(destination->dc, layout.map_x, layout.map_y,
               layout.draw_w, layout.draw_h, source->dc, 0, 0,
               source->width, source->height, SRCCOPY);
    RestoreDC(destination->dc, saved);
}

static int build_canonical_layers(
    StaticPhysicalProbeCanvas *base, StaticPhysicalProbeCanvas *coast,
    StaticPhysicalProbeCanvas *climate,
    const RenderSnapshot *snapshot) {
    RECT full;
    MapLayout layout;
    if (!map_shore_color_cache_prepare(snapshot)) return 0;
    if (!static_physical_probe_canvas_open(
            base, snapshot->map_w * CANONICAL_SCALE,
            snapshot->map_h * CANONICAL_SCALE) ||
        !static_physical_probe_canvas_open(
            coast, snapshot->map_w * CANONICAL_SCALE,
            snapshot->map_h * CANONICAL_SCALE) ||
        !static_physical_probe_canvas_open(
            climate, snapshot->map_w * CANONICAL_SCALE,
            snapshot->map_h * CANONICAL_SCALE)) return 0;
    if (!render_static_physical_cache_prewarm(base->dc, snapshot) ||
        !render_static_physical_cache_compose_base_coast(
            base->dc, DISPLAY_GEOGRAPHY) ||
        !render_static_physical_cache_compose_base_coast(
            climate->dc, DISPLAY_CLIMATE)) return 0;
    GdiFlush();
    memcpy(coast->pixels, base->pixels,
           (size_t)base->width * (size_t)base->height * sizeof(*base->pixels));
    full = (RECT){0, 0, coast->width, coast->height};
    layout = (MapLayout){0, 0, CANONICAL_SCALE,
                         coast->width, coast->height};
    coast_geometry_draw_fill(coast->dc, full, layout, snapshot,
                             DISPLAY_GEOGRAPHY);
    GdiFlush();
    return 1;
}

static int write_layer(const StaticPhysicalProbeCanvas *canvas, int zoom,
                       const char *suffix) {
    char name[112];
    snprintf(name, sizeof(name), "static_coast_artifact_%s_%d.bmp",
             suffix, zoom);
    return static_physical_probe_canvas_write(
        canvas, static_physical_probe_artifact_dir(), name);
}

static void report_layer(
    FILE *summary, int zoom, const char *layer, const char *artifact,
    const CoastArtifactIsolationMetrics *metrics) {
    int gated = strcmp(layer, "river") != 0;
    fprintf(summary,
            "case=static_coast_artifact_layer zoom=%d layer=%s ok=%d gated=%d "
            "changed=%d exact=%d/%d near=%d/%d family=%d/%d "
            "family_class_island=%d family_class_wetland=%d "
            "candidates=%d land=%d ocean=%d lake=%d coast_leak=%d "
            "water_land_leak=%d geometry_leak=%d thin_runs=%d "
            "comb_clusters=%d artifact=%s\n",
            zoom, layer, !gated || metrics->comb_clusters == 0, gated,
            metrics->changed_pixels, metrics->exact_cyan_pixels,
            metrics->exact_green_pixels, metrics->near_cyan_pixels,
            metrics->near_green_pixels, metrics->family_cyan_pixels,
            metrics->family_green_pixels, metrics->family_island_pixels,
            metrics->family_wetland_pixels, metrics->candidate_pixels,
            metrics->candidate_land_pixels, metrics->candidate_ocean_pixels,
            metrics->candidate_lake_pixels, metrics->coast_mask_leak_pixels,
            metrics->water_land_leak_pixels, metrics->geometry_leak_pixels,
            metrics->thin_runs,
            metrics->comb_clusters, artifact);
}

static int coverage_preservation_contract(
    FILE *summary, const RenderSnapshot *snapshot) {
    const RenderWaterCoverageStats *stats;
    RenderWaterCoastPresentationMetrics coast;
    unsigned char *categories = NULL;
    size_t tile_count;
    int scale;
    int water_centers = 0;
    int land_centers = 0;
    int center_failures = 0;
    int overlap_pixels = 0;
    int sum_failures = 0;
    int semantic_lake_failures = 0;
    int presentation_changes = 0;
    int x, y;
    tile_count = (size_t)snapshot->map_w * (size_t)snapshot->map_h;
    categories = (unsigned char *)malloc(tile_count);
    if (!categories || !render_water_coast_presentation_build(
            snapshot, categories, &coast) ||
        !render_water_coverage_prepare(snapshot)) {
        free(categories);
        return 0;
    }
    scale = render_water_coverage_scale();
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            int geography = snapshot->tiles[
                y * snapshot->map_w + x].geography;
            int semantic = geography == GEO_LAKE ? 2 :
                (geography == GEO_OCEAN || geography == GEO_BAY ? 1 : 0);
            int expected = categories[y * snapshot->map_w + x];
            int dx, dy;
            semantic_lake_failures += geography == GEO_LAKE && expected != 2;
            presentation_changes += expected != semantic;
            if (expected) water_centers++;
            else land_centers++;
            for (dy = scale / 2 - 1; dy <= scale / 2; dy++) {
                for (dx = scale / 2 - 1; dx <= scale / 2; dx++) {
                    int px = x * scale + dx;
                    int py = y * scale + dy;
                    int ocean = render_water_coverage_ocean_alpha(px, py);
                    int lake = render_water_coverage_lake_alpha(px, py);
                    int actual = lake == 255 && ocean == 0 ? 2 :
                        (ocean == 255 && lake == 0 ? 1 : 0);
                    center_failures += actual != expected;
                }
            }
        }
    }
    for (y = 0; y < render_water_coverage_height(); y++) {
        for (x = 0; x < render_water_coverage_width(); x++) {
            int ocean = render_water_coverage_ocean_alpha(x, y);
            int lake = render_water_coverage_lake_alpha(x, y);
            overlap_pixels += ocean > 0 && lake > 0;
            sum_failures += ocean + lake > 255;
        }
    }
    stats = render_water_coverage_stats();
    fprintf(summary,
            "case=static_water_coverage_preservation ok=%d scale=%d "
            "water_centers=%d land_centers=%d center_failures=%d "
            "overlap_pixels=%d sum_failures=%d ocean_tiles=%d "
            "lake_tiles=%d lake_land_rejected=%llu "
            "ocean_land_rejected=%llu presentation_changes=%d "
            "removed=%llu filled=%llu cleanup=%llu lake_failures=%d "
            "hash=%llu retained_bytes=%llu\n",
            center_failures == 0 && overlap_pixels == 0 && sum_failures == 0 &&
                semantic_lake_failures == 0 &&
                presentation_changes == 0 &&
                coast.removed_ocean_tiles == 0 &&
                coast.filled_land_tiles == 0 &&
                coast.ocean_cleanup_tiles == 0 &&
                stats->coast_presentation_hash == coast.presentation_hash,
            scale, water_centers, land_centers, center_failures,
            overlap_pixels, sum_failures, stats->ocean_tiles,
            stats->lake_tiles,
            (unsigned long long)stats->lake_land_pixels_rejected,
            (unsigned long long)stats->ocean_land_pixels_rejected,
            presentation_changes,
            (unsigned long long)coast.removed_ocean_tiles,
            (unsigned long long)coast.filled_land_tiles,
            (unsigned long long)coast.ocean_cleanup_tiles,
            semantic_lake_failures,
            (unsigned long long)coast.presentation_hash,
            (unsigned long long)stats->retained_bytes);
    free(categories);
    return center_failures == 0 && overlap_pixels == 0 &&
           sum_failures == 0 && semantic_lake_failures == 0 &&
           presentation_changes == 0 && coast.removed_ocean_tiles == 0 &&
           coast.filled_land_tiles == 0 && coast.ocean_cleanup_tiles == 0 &&
           stats->coast_presentation_hash == coast.presentation_hash;
}

static int shoreline_texture_contract(FILE *summary) {
    const RenderWaterSurfaceCacheStats *water =
        render_water_surface_cache_stats();
    int ok = water->texture_inset_radius == 0 &&
             water->ocean_partial_coverage_pixels > 0 &&
             (water->lake_tiles > 0 ?
                  water->lake_partial_coverage_pixels > 0 :
                  water->lake_partial_coverage_pixels == 0) &&
             water->coast_regularized_components == 0 &&
             water->coast_regularized_tiles == 0 &&
             water->coast_regularized_pixels == 0 &&
             water->coast_removed_ocean_tiles == 0 &&
             water->coast_removed_ocean_pixels == 0 &&
             water->coast_filled_land_tiles == 0 &&
             water->coast_filled_land_pixels == 0 &&
             water->coast_cleanup_tiles == 0 &&
             water->coast_candidate_comparisons == 0;
    fprintf(summary,
            "case=static_shoreline_texture_source_authoritative ok=%d radius=%d "
            "ocean_partial_pixels=%llu lake_partial_pixels=%llu "
            "thin=%llu one_ended=%llu preserved=%llu regularized=%llu "
            "sparse_mesh=%llu/%llu "
            "tiles=%llu pixels=%llu removed=%llu/%llu filled=%llu/%llu cleanup=%llu "
            "protected=%llu hash=%llu comparisons=%llu transient_bytes=%llu\n",
            ok, water->texture_inset_radius,
            (unsigned long long)water->ocean_partial_coverage_pixels,
            (unsigned long long)water->lake_partial_coverage_pixels,
            (unsigned long long)water->coast_thin_components,
            (unsigned long long)water->coast_one_ended_components,
            (unsigned long long)water->coast_preserved_components,
            (unsigned long long)water->coast_regularized_components,
            (unsigned long long)water->coast_sparse_mesh_components,
            (unsigned long long)water->coast_sparse_mesh_tiles,
            (unsigned long long)water->coast_regularized_tiles,
            (unsigned long long)water->coast_regularized_pixels,
            (unsigned long long)water->coast_removed_ocean_tiles,
            (unsigned long long)water->coast_removed_ocean_pixels,
            (unsigned long long)water->coast_filled_land_tiles,
            (unsigned long long)water->coast_filled_land_pixels,
            (unsigned long long)water->coast_cleanup_tiles,
            (unsigned long long)water->coast_protected_land_tiles,
            (unsigned long long)water->coast_presentation_hash,
            (unsigned long long)water->coast_candidate_comparisons,
            (unsigned long long)water->coast_smoothing_transient_bytes);
    return ok;
}

static int render_zoom(FILE *summary, StaticPhysicalProbeCanvas *canvas,
                       const StaticPhysicalProbeCanvas *base,
                       const StaticPhysicalProbeCanvas *coast,
                       const StaticPhysicalProbeCanvas *climate,
                       const RenderSnapshot *snapshot,
                       const unsigned char *categories, int zoom,
                       uint32_t *previous, unsigned char *mask) {
    static const char *names[COAST_ARTIFACT_LAYER_COUNT] = {
        "base", "base_coast", "final_water_without_rivers",
        "base_coast_ocean", "river_only"
    };
    static const char *labels[COAST_ARTIFACT_LAYER_COUNT] = {
        "base", "coast", "lake", "ocean", "river"
    };
    CoastArtifactIsolationMetrics metrics[COAST_ARTIFACT_LAYER_COUNT];
    CoastArtifactIsolationMetrics final_metrics;
    char final_artifact[112];
    RECT client = {0, 0, canvas->width, canvas->height};
    MapLayout layout = centered_layout(snapshot, client, zoom);
    size_t bytes = (size_t)canvas->width * (size_t)canvas->height *
                   sizeof(*canvas->pixels);
    int lod = river_render_lod_bucket_for_zoom(zoom, layout.tile_size);
    int artifact_ok = 1;
    int layers_ok = 1;
    int i;
    static_physical_probe_canvas_clear(canvas);
    present_canonical(canvas, base, client, layout);
    GdiFlush();
    layers_ok &= game_presentation_coast_artifact_analyze(
        canvas, snapshot, categories, layout, COAST_ARTIFACT_LAYER_BASE,
        NULL, mask, &metrics[COAST_ARTIFACT_LAYER_BASE]);
    artifact_ok &= write_layer(canvas, zoom,
                               names[COAST_ARTIFACT_LAYER_BASE]);
    memcpy(previous, canvas->pixels, bytes);

    static_physical_probe_canvas_clear(canvas);
    present_canonical(canvas, coast, client, layout);
    GdiFlush();
    layers_ok &= game_presentation_coast_artifact_analyze(
        canvas, snapshot, categories, layout, COAST_ARTIFACT_LAYER_COAST,
        previous, mask, &metrics[COAST_ARTIFACT_LAYER_COAST]);
    artifact_ok &= write_layer(canvas, zoom,
                               names[COAST_ARTIFACT_LAYER_COAST]);
    memcpy(previous, canvas->pixels, bytes);

    render_water_surface_cache_present_ocean(canvas->dc, client, layout,
                                             snapshot);
    GdiFlush();
    layers_ok &= game_presentation_coast_artifact_analyze(
        canvas, snapshot, categories, layout, COAST_ARTIFACT_LAYER_OCEAN,
        previous, mask, &metrics[COAST_ARTIFACT_LAYER_OCEAN]);
    artifact_ok &= write_layer(canvas, zoom,
                               names[COAST_ARTIFACT_LAYER_OCEAN]);
    memcpy(previous, canvas->pixels, bytes);

    render_water_surface_cache_present_lake(canvas->dc, client, layout,
                                            snapshot);
    GdiFlush();
    layers_ok &= game_presentation_coast_artifact_analyze(
        canvas, snapshot, categories, layout, COAST_ARTIFACT_LAYER_LAKE,
        previous, mask, &metrics[COAST_ARTIFACT_LAYER_LAKE]);
    artifact_ok &= write_layer(canvas, zoom,
                               names[COAST_ARTIFACT_LAYER_LAKE]);
    /* Production presents ocean first and the lake surface second. Attribute
       the final water gate to that final lake pass. */
    final_metrics = metrics[COAST_ARTIFACT_LAYER_LAKE];
    snprintf(final_artifact, sizeof(final_artifact),
             "static_coast_artifact_final_water_without_rivers_%d.bmp", zoom);
    report_layer(summary, zoom, "final_composite", final_artifact,
                 &final_metrics);

    static_physical_probe_canvas_clear(canvas);
    GdiFlush();
    memcpy(previous, canvas->pixels, bytes);
    if (!render_static_physical_overlay_cache_ensure_river(
            canvas->dc, snapshot, lod)) artifact_ok = 0;
    render_static_physical_overlay_cache_present_river(
        canvas->dc, client, layout, snapshot, lod);
    GdiFlush();
    game_presentation_coast_artifact_analyze(
        canvas, snapshot, categories, layout, COAST_ARTIFACT_LAYER_RIVER,
        previous, mask, &metrics[COAST_ARTIFACT_LAYER_RIVER]);
    artifact_ok &= write_layer(canvas, zoom,
                               names[COAST_ARTIFACT_LAYER_RIVER]);

    for (i = 0; i < COAST_ARTIFACT_LAYER_COUNT; i++) {
        char artifact[112];
        snprintf(artifact, sizeof(artifact),
                 "static_coast_artifact_%s_%d.bmp", names[i], zoom);
        report_layer(summary, zoom, labels[i], artifact, &metrics[i]);
    }
    {
        CoastArtifactIsolationMetrics climate_metrics;
        char climate_artifact[112];
        static_physical_probe_canvas_clear(canvas);
        present_canonical(canvas, climate, client, layout);
        GdiFlush();
        layers_ok &= game_presentation_coast_artifact_analyze(
            canvas, snapshot, categories, layout, COAST_ARTIFACT_LAYER_BASE,
            NULL, mask, &climate_metrics);
        artifact_ok &= write_layer(canvas, zoom, "climate_base");
        snprintf(climate_artifact, sizeof(climate_artifact),
                 "static_coast_artifact_climate_base_%d.bmp", zoom);
        report_layer(summary, zoom, "climate",
                     climate_artifact,
                     &climate_metrics);
    }
    fprintf(summary,
            "case=static_coast_artifact_isolation zoom=%d ok=%d "
            "hydro_comb=%d coast_comb=%d lake_comb=%d ocean_comb=%d final_comb=%d "
            "river_comb=%d coast_mask_palette_leak=%d "
            "water_land_palette_leak=%d lod=%d\n",
            zoom, layers_ok && artifact_ok,
            metrics[COAST_ARTIFACT_LAYER_BASE].comb_clusters,
            metrics[COAST_ARTIFACT_LAYER_COAST].comb_clusters,
            metrics[COAST_ARTIFACT_LAYER_LAKE].comb_clusters,
            metrics[COAST_ARTIFACT_LAYER_OCEAN].comb_clusters,
            final_metrics.comb_clusters,
            metrics[COAST_ARTIFACT_LAYER_RIVER].comb_clusters,
            metrics[COAST_ARTIFACT_LAYER_COAST].coast_mask_leak_pixels,
            metrics[COAST_ARTIFACT_LAYER_LAKE].water_land_leak_pixels +
                metrics[COAST_ARTIFACT_LAYER_OCEAN].water_land_leak_pixels,
            lod);
    return layers_ok && artifact_ok;
}

int game_presentation_coast_artifact_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot) {
    static const int zooms[] = {
        25, 50, 65, 100, 135, 150, 200, 225, 300, 700
    };
    StaticPhysicalProbeCanvas base = {0};
    StaticPhysicalProbeCanvas coast = {0};
    StaticPhysicalProbeCanvas climate = {0};
    uint32_t *previous = NULL;
    unsigned char *mask = NULL;
    unsigned char *categories = NULL;
    RenderWaterCoastPresentationMetrics coast_metrics = {0};
    size_t pixel_count;
    int detector_comb = 0;
    int detector_single = 0;
    int detector_ok;
    CoastArtifactSaddleMetrics saddle = {0};
    int saddle_ok;
    int render_ok = 1;
    int canonical_ok = 0;
    int i;
    if (!summary || !canvas || !canvas->pixels || !snapshot ||
        !snapshot->world_generated) return 0;
    detector_ok = game_presentation_coast_artifact_detector_contract(
        &detector_comb, &detector_single);
    fprintf(summary,
            "case=static_coast_artifact_detector_contract ok=%d "
            "rejected_cyan=70/145/190 rejected_green=169/203/123 "
            "comb_clusters=%d single_river_clusters=%d tolerance=%d\n",
            detector_ok, detector_comb, detector_single,
            COAST_ARTIFACT_REJECT_TOLERANCE);
    saddle_ok = game_presentation_coast_artifact_saddle_contract(
        snapshot, &saddle);
    fprintf(summary,
            "case=static_coast_artifact_saddle_contract ok=%d "
            "ocean_ambiguous=%d ocean_checker=%d ocean_fractional=%d "
            "lake_ambiguous=%d lake_checker=%d lake_fractional=%d\n",
            saddle_ok, saddle.ocean_ambiguous_cells,
            saddle.ocean_checker_cells, saddle.ocean_fractional_cells,
            saddle.lake_ambiguous_cells, saddle.lake_checker_cells,
            saddle.lake_fractional_cells);
    pixel_count = (size_t)canvas->width * (size_t)canvas->height;
    previous = (uint32_t *)malloc(pixel_count * sizeof(*previous));
    mask = (unsigned char *)malloc(pixel_count);
    categories = (unsigned char *)malloc(
        (size_t)snapshot->map_w * (size_t)snapshot->map_h);
    canonical_ok = previous && mask && categories &&
        render_water_coast_presentation_build(
            snapshot, categories, &coast_metrics) &&
        coast_metrics.removed_ocean_tiles == 0 &&
        coast_metrics.filled_land_tiles == 0 &&
        coast_metrics.ocean_cleanup_tiles == 0 &&
        build_canonical_layers(&base, &coast, &climate, snapshot) &&
        game_presentation_water_edge_base_contract(summary, &base, snapshot) &&
        render_water_surface_cache_ensure(canvas->dc, snapshot) &&
        coverage_preservation_contract(summary, snapshot) &&
        shoreline_texture_contract(summary);
    fprintf(summary,
            "case=static_coast_category_one_sided ok=%d removed=%llu "
            "filled=%llu cleanup=%llu hash=%llu\n",
            coast_metrics.removed_ocean_tiles == 0 &&
                coast_metrics.filled_land_tiles == 0 &&
                coast_metrics.ocean_cleanup_tiles == 0,
            (unsigned long long)coast_metrics.removed_ocean_tiles,
            (unsigned long long)coast_metrics.filled_land_tiles,
            (unsigned long long)coast_metrics.ocean_cleanup_tiles,
            (unsigned long long)coast_metrics.presentation_hash);
    if (canonical_ok) {
        for (i = 0; i < (int)(sizeof(zooms) / sizeof(zooms[0])); i++)
            render_ok &= render_zoom(summary, canvas, &base, &coast, &climate,
                                     snapshot, categories, zooms[i],
                                     previous, mask);
    } else {
        fprintf(summary,
                "case=static_coast_artifact_isolation ok=0 reason=setup\n");
        render_ok = 0;
    }
    free(mask);
    free(previous);
    free(categories);
    static_physical_probe_canvas_close(&climate);
    static_physical_probe_canvas_close(&coast);
    static_physical_probe_canvas_close(&base);
    return detector_ok && saddle_ok && canonical_ok && render_ok;
}
