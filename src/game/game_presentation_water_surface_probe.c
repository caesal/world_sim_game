#include "game/game_presentation_water_surface_probe.h"
#include "game/game_presentation_water_reference_probe.h"
#include "game/game_presentation_water_legend_probe.h"

#include "core/game_types.h"
#include "render/map_display_policy.h"
#include "render/render_common.h"
#include "render/render_context.h"
#include "render/render_ocean_assets.h"
#include "render/render_ocean_coverage.h"
#include "render/render_ocean_decoration.h"
#include "render/render_ocean_decoration_water.h"
#include "render/render_static_scene.h"
#include "render/render_water_coast_presentation.h"
#include "render/render_water_coverage.h"
#include "render/render_water_surface_cache.h"
#include "ui/ui_layout.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int display_mode;
    int map_zoom_percent;
    int map_offset_x;
    int map_offset_y;
    int map_view_auto_centered;
    int map_interaction_preview;
    int auto_run;
} WaterProbeUiState;

typedef struct {
    int zoom;
    int lake_pixels;
    int lake_flat_pixels;
    int lake_unique_colors;
    int suspicious_pixels;
    int coast_outline_pixels;
    int coast_color_blocks;
    int raw_coast_pixels;
    int obsolete_band_pixels;
    int nonblank_pixels;
    uint64_t lake_hash;
    uint64_t ocean_hash;
    uint64_t bay_hash;
    uint64_t shallow_hash;
    uint64_t lake_red;
    uint64_t lake_green;
    uint64_t lake_blue;
    int stable;
} WaterZoomMetrics;

typedef struct {
    int samples;
    int leaks;
    int lake_samples;
    int lake_leaks;
    int category_lake_mismatches;
    int stats_consistent;
    uint64_t shared_hash;
    uint64_t cached_hash;
    uint64_t removed_ocean_tiles;
    uint64_t filled_land_tiles;
    uint64_t protected_land_tiles;
} WaterCoverageContractMetrics;

static WaterProbeUiState save_ui(void) {
    WaterProbeUiState state;
    state.display_mode = display_mode;
    state.map_zoom_percent = map_zoom_percent;
    state.map_offset_x = map_offset_x;
    state.map_offset_y = map_offset_y;
    state.map_view_auto_centered = map_view_auto_centered;
    state.map_interaction_preview = map_interaction_preview;
    state.auto_run = auto_run;
    return state;
}

static void restore_ui(WaterProbeUiState state) {
    display_mode = state.display_mode;
    map_zoom_percent = state.map_zoom_percent;
    map_offset_x = state.map_offset_x;
    map_offset_y = state.map_offset_y;
    map_view_auto_centered = state.map_view_auto_centered;
    map_interaction_preview = state.map_interaction_preview;
    auto_run = state.auto_run;
}

static uint32_t dib_color(COLORREF color) {
    return (uint32_t)GetBValue(color) | ((uint32_t)GetGValue(color) << 8) |
           ((uint32_t)GetRValue(color) << 16);
}

static uint64_t mix_hash(uint64_t hash, uint32_t value) {
    hash ^= value;
    return hash * UINT64_C(1099511628211);
}

static int screen_tile(const RenderSnapshot *snapshot, MapLayout layout,
                       int px, int py, int *x, int *y) {
    int draw_w = layout.draw_w > 0 ? layout.draw_w : 1;
    int draw_h = layout.draw_h > 0 ? layout.draw_h : 1;
    if (px < layout.map_x || py < layout.map_y ||
        px >= layout.map_x + layout.draw_w ||
        py >= layout.map_y + layout.draw_h) return 0;
    *x = (px - layout.map_x) * snapshot->map_w / draw_w;
    *y = (py - layout.map_y) * snapshot->map_h / draw_h;
    return *x >= 0 && *y >= 0 && *x < snapshot->map_w &&
           *y < snapshot->map_h;
}

static void note_unique(uint32_t color, uint32_t *colors, int *count) {
    int i;
    for (i = 0; i < *count; i++) if (colors[i] == color) return;
    if (*count < 64) colors[(*count)++] = color;
}

static void analyze_canvas(const StaticPhysicalProbeCanvas *canvas,
                           const RenderSnapshot *snapshot, MapLayout layout,
                           WaterZoomMetrics *out) {
    uint32_t unique[64];
    uint32_t flat = dib_color(RGB(78, 151, 188));
    uint32_t magenta = dib_color(RGB(255, 0, 255));
    uint32_t white = dib_color(RGB(255, 255, 255));
    uint32_t coast = dib_color(RGB(82, 151, 168));
    uint32_t raw_coast = dib_color(RGB(199, 224, 201));
    uint32_t obsolete_band = dib_color(RGB(174, 202, 188));
    RECT viewport = get_map_content_rect(
        (RECT){0, 0, canvas->width, canvas->height});
    int unique_count = 0;
    int px, py;
    memset(unique, 0, sizeof(unique));
    out->lake_hash = out->ocean_hash = out->bay_hash = out->shallow_hash =
        UINT64_C(1469598103934665603);
    for (py = viewport.top; py < viewport.bottom; py++) {
        for (px = viewport.left; px < viewport.right; px++) {
            uint32_t color = canvas->pixels[py * canvas->width + px] &
                             UINT32_C(0x00ffffff);
            const SnapshotTile *tile;
            int x, y;
            if (!screen_tile(snapshot, layout, px, py, &x, &y)) continue;
            tile = &snapshot->tiles[y * snapshot->map_w + x];
            if (color != 0) out->nonblank_pixels++;
            out->raw_coast_pixels += color == raw_coast;
            out->obsolete_band_pixels += color == obsolete_band;
            out->coast_outline_pixels += color == coast;
            if (px + 1 < viewport.right && py + 1 < viewport.bottom &&
                color == coast &&
                (canvas->pixels[py * canvas->width + px + 1] &
                 UINT32_C(0x00ffffff)) == coast &&
                (canvas->pixels[(py + 1) * canvas->width + px] &
                 UINT32_C(0x00ffffff)) == coast &&
                (canvas->pixels[(py + 1) * canvas->width + px + 1] &
                 UINT32_C(0x00ffffff)) == coast)
                out->coast_color_blocks++;
            if (tile->geography == GEO_LAKE) {
                out->lake_pixels++;
                out->lake_flat_pixels += color == flat;
                out->suspicious_pixels += color == magenta || color == white;
                out->lake_red += (color >> 16) & 255u;
                out->lake_green += (color >> 8) & 255u;
                out->lake_blue += color & 255u;
                out->lake_hash = mix_hash(out->lake_hash, color);
                note_unique(color, unique, &unique_count);
            } else if (tile->geography == GEO_OCEAN) {
                out->ocean_hash = mix_hash(out->ocean_hash, color);
            } else if (tile->geography == GEO_BAY) {
                out->bay_hash = mix_hash(out->bay_hash, color);
            }
            if ((tile->geography == GEO_OCEAN ||
                 tile->geography == GEO_BAY) &&
                tile->water_depth == WATER_DEPTH_SHALLOW)
                out->shallow_hash = mix_hash(out->shallow_hash, color);
        }
    }
    out->lake_unique_colors = unique_count;
}

static int metrics_same(const WaterZoomMetrics *a,
                        const WaterZoomMetrics *b) {
    return a->lake_pixels == b->lake_pixels &&
           a->lake_hash == b->lake_hash &&
           a->ocean_hash == b->ocean_hash && a->bay_hash == b->bay_hash &&
           a->shallow_hash == b->shallow_hash;
}

static int coverage_locality_contract(
    const RenderSnapshot *snapshot, WaterCoverageContractMetrics *out) {
    RenderWaterCoastPresentationMetrics shared = {0};
    const RenderWaterCoverageStats *cached;
    MapLayout canonical = {0, 0, 2, snapshot->map_w * 2,
                           snapshot->map_h * 2};
    size_t tile_count = (size_t)snapshot->map_w *
                        (size_t)snapshot->map_h;
    unsigned char *categories = (unsigned char *)malloc(tile_count);
    int ok = 0;
    int x, y, dx, dy;
    memset(out, 0, sizeof(*out));
    if (!categories || !render_water_coast_presentation_build(
                           snapshot, categories, &shared) ||
        !render_ocean_coverage_prepare_field(snapshot)) goto cleanup;
    cached = render_water_coverage_stats();
    out->shared_hash = shared.presentation_hash;
    out->cached_hash = cached->coast_presentation_hash;
    out->removed_ocean_tiles = shared.removed_ocean_tiles;
    out->filled_land_tiles = shared.filled_land_tiles;
    out->protected_land_tiles = shared.protected_land_tiles +
                                shared.protected_lake_neighbor_tiles;
    out->stats_consistent =
        cached->coast_removed_ocean_tiles == shared.removed_ocean_tiles &&
        cached->coast_filled_land_tiles == shared.filled_land_tiles &&
        cached->coast_protected_land_tiles == shared.protected_land_tiles &&
        cached->coast_protected_lake_neighbor_tiles ==
            shared.protected_lake_neighbor_tiles &&
        cached->coast_presentation_hash == shared.presentation_hash &&
        cached->coast_presentation_transient_bytes == shared.transient_bytes &&
        (uint64_t)cached->ocean_tiles == shared.semantic_ocean_tiles &&
        (uint64_t)cached->lake_tiles == shared.semantic_lake_tiles;
    for (y = 0; y + 1 < snapshot->map_h; y++) {
        for (x = 0; x + 1 < snapshot->map_w; x++) {
            int a = categories[y * snapshot->map_w + x];
            int b = categories[y * snapshot->map_w + x + 1];
            int c = categories[(y + 1) * snapshot->map_w + x];
            int d = categories[(y + 1) * snapshot->map_w + x + 1];
            int expected = a == WATER_COAST_PRESENTATION_OCEAN;
            if (b != a || c != a || d != a) continue;
            for (dy = 0; dy < 2; dy++) {
                for (dx = 0; dx < 2; dx++) {
                    int actual = render_ocean_coverage_pixel_is_ocean(
                        snapshot, canonical, x * 2 + 1 + dx,
                        y * 2 + 1 + dy);
                    out->samples++;
                    out->leaks += actual != expected;
                }
            }
        }
    }
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            int index = y * snapshot->map_w + x;
            int semantic_lake =
                snapshot->tiles[index].geography == GEO_LAKE;
            out->category_lake_mismatches +=
                semantic_lake !=
                (categories[index] == WATER_COAST_PRESENTATION_LAKE);
            if (semantic_lake) {
                for (dy = 0; dy < 2; dy++) {
                    for (dx = 0; dx < 2; dx++) {
                        out->lake_samples++;
                        out->lake_leaks +=
                            render_ocean_coverage_pixel_is_ocean(
                                snapshot, canonical, x * 2 + dx,
                                y * 2 + dy) != 0;
                    }
                }
            }
        }
    }
    ok = out->samples > 0 && out->leaks == 0 &&
         out->lake_samples > 0 && out->lake_leaks == 0 &&
         out->category_lake_mismatches == 0 && out->stats_consistent;
cleanup:
    free(categories);
    return ok;
}

static int draw_zoom(StaticPhysicalProbeCanvas *canvas,
                     const RenderSnapshot *snapshot, int zoom,
                     WaterZoomMetrics *metrics) {
    WaterZoomMetrics first = {0}, second = {0};
    char name[80];
    RECT client = {0, 0, canvas->width, canvas->height};
    MapLayout layout;
    display_mode = DISPLAY_GEOGRAPHY;
    map_zoom_percent = zoom;
    map_offset_x = map_offset_y = 0;
    map_view_auto_centered = 1;
    map_interaction_preview = 0;
    auto_run = 0;
    layout = get_map_layout(client);
    first.zoom = second.zoom = zoom;
    static_physical_probe_canvas_clear(canvas);
    render_context_begin(snapshot);
    render_static_scene_draw(canvas->dc, client, layout, snapshot);
    render_context_end();
    GdiFlush();
    analyze_canvas(canvas, snapshot, layout, &first);
    static_physical_probe_canvas_clear(canvas);
    render_context_begin(snapshot);
    render_static_scene_draw(canvas->dc, client, layout, snapshot);
    render_context_end();
    GdiFlush();
    analyze_canvas(canvas, snapshot, layout, &second);
    second.stable = metrics_same(&first, &second);
    *metrics = second;
    snprintf(name, sizeof(name), "static_water_river_geography_%d.bmp", zoom);
    return static_physical_probe_canvas_write(
        canvas, static_physical_probe_artifact_dir(), name);
}

static uint64_t water_semantics_hash(const RenderSnapshot *snapshot) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    hash = mix_hash(hash, (uint32_t)snapshot->map_w);
    hash = mix_hash(hash, (uint32_t)snapshot->map_h);
    for (i = 0; i < snapshot->map_w * snapshot->map_h; i++) {
        const SnapshotTile *tile = &snapshot->tiles[i];
        uint32_t packed = (uint32_t)tile->geography |
                          ((uint32_t)tile->water_depth << 8) |
                          ((uint32_t)tile->water_deep_percent << 16);
        hash = mix_hash(hash, packed);
    }
    return hash;
}

static int water_semantics_contract(const RenderSnapshot *snapshot,
                                    int *lakes, int *oceans, int *bays,
                                    int *shallow,
                                    int *lake_motifs_excluded) {
    int i;
    int ok = 1;
    *lakes = *oceans = *bays = *shallow = 0;
    *lake_motifs_excluded = 1;
    for (i = 0; i < snapshot->map_w * snapshot->map_h; i++) {
        const SnapshotTile *tile = &snapshot->tiles[i];
        int x = i % snapshot->map_w;
        int y = i / snapshot->map_w;
        if (tile->geography == GEO_LAKE) {
            COLORREF geography = map_display_policy_snapshot_physical_color(
                tile, MAP_PHYSICAL_BASE_GEOGRAPHY);
            COLORREF climate = map_display_policy_snapshot_physical_color(
                tile, MAP_PHYSICAL_BASE_CLIMATE);
            (*lakes)++;
            *lake_motifs_excluded &=
                !ocean_decoration_water_tile(snapshot, x, y);
            ok &= geography == RGB(78, 151, 188) &&
                  climate == geography &&
                  *lake_motifs_excluded;
        } else if (tile->geography == GEO_OCEAN ||
                   tile->geography == GEO_BAY) {
            *oceans += tile->geography == GEO_OCEAN;
            *bays += tile->geography == GEO_BAY;
            *shallow += tile->water_depth == WATER_DEPTH_SHALLOW;
            ok &= ocean_decoration_water_tile(snapshot, x, y);
        }
    }
    return ok && *lakes > 0 && *oceans > 0 && *shallow > 0;
}

static int tint_value_contract(void) {
    return render_water_surface_lake_tint_pixel(UINT32_C(0xff3787c7)) ==
               UINT32_C(0xff69acde) &&
           render_water_surface_lake_tint_pixel(UINT32_C(0xff000000)) ==
               UINT32_C(0xff455e7f) &&
           render_water_surface_lake_tint_pixel(UINT32_C(0xffffffff)) ==
               UINT32_C(0xffedf1f9) &&
           render_water_surface_lake_tint_pixel(UINT32_C(0xff367eb8)) ==
               UINT32_C(0xff68a7d7);
}

static int water_ready_identity_contract(const RenderSnapshot *snapshot,
                                         int *current, int *stale,
                                         int *restored) {
    RenderSnapshot *mutable_snapshot = (RenderSnapshot *)(uintptr_t)snapshot;
    int region_count = snapshot->region_count;
    *current = render_water_surface_cache_ready(snapshot);
    mutable_snapshot->region_count = region_count + 1;
    *stale = !render_water_surface_cache_ready(snapshot);
    mutable_snapshot->region_count = region_count;
    *restored = render_water_surface_cache_ready(snapshot);
    return *current && *stale && *restored;
}

int game_presentation_water_surface_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot) {
    static const int zooms[] = {25, 50, 100, 200, 700};
    WaterZoomMetrics metrics[5] = {{0}};
    WaterProbeUiState old_ui = save_ui();
    uint64_t semantics_before, semantics_after;
    int lake_count, ocean_count, bay_count, shallow_count;
    int lake_motifs_excluded;
    int pattern_matches, exact_matches, pattern_pixels, raw_unique;
    int pattern_max_delta;
    WaterCoverageContractMetrics coverage = {0};
    int ready_current, ready_stale, ready_restored;
    int contract_ok, tint_ok, geometry_ok, coverage_ok, identity_ok;
    int zoom_ok = 1, artifact_ok = 1, legend_ok, pattern_frames = 0;
    int i;
    if (!summary || !canvas || !snapshot) return 0;
    semantics_before = water_semantics_hash(snapshot);
    contract_ok = water_semantics_contract(
        snapshot, &lake_count, &ocean_count, &bay_count, &shallow_count,
        &lake_motifs_excluded);
    tint_ok = tint_value_contract();
    coverage_ok = coverage_locality_contract(snapshot, &coverage);
    for (i = 0; i < 5; i++) {
        int pattern_ok = 1;
        artifact_ok &= draw_zoom(canvas, snapshot, zooms[i], &metrics[i]);
        if (metrics[i].lake_pixels > 100) {
            pattern_frames++;
            pattern_ok = metrics[i].lake_unique_colors >= 4 &&
                         metrics[i].lake_flat_pixels * 2 <
                             metrics[i].lake_pixels &&
                         metrics[i].lake_blue > metrics[i].lake_green &&
                         metrics[i].lake_green > metrics[i].lake_red;
        }
        zoom_ok &= metrics[i].stable && metrics[i].nonblank_pixels > 0 &&
                   metrics[i].suspicious_pixels == 0 && pattern_ok &&
                   metrics[i].coast_outline_pixels == 0 &&
                   metrics[i].obsolete_band_pixels == 0;
        if (zooms[i] >= 200)
            zoom_ok &= metrics[i].coast_color_blocks <= 8 &&
                       metrics[i].raw_coast_pixels <= 100;
        fprintf(summary,
                "case=static_water_river_zoom zoom=%d ok=%d stable=%d "
                "lake_pixels=%d old_flat=%d unique=%d suspicious=%d "
                "coast_outline=%d coast_blocks=%d raw_coast=%d "
                "obsolete_band=%d hashes=%llu/%llu/%llu/%llu "
                "artifact=static_water_river_geography_%d.bmp\n",
                zooms[i], metrics[i].stable && pattern_ok &&
                    metrics[i].suspicious_pixels == 0 &&
                    metrics[i].coast_outline_pixels == 0 &&
                    metrics[i].obsolete_band_pixels == 0,
                metrics[i].stable, metrics[i].lake_pixels,
                metrics[i].lake_flat_pixels, metrics[i].lake_unique_colors,
                metrics[i].suspicious_pixels,
                metrics[i].coast_outline_pixels,
                metrics[i].coast_color_blocks, metrics[i].raw_coast_pixels,
                metrics[i].obsolete_band_pixels,
                (unsigned long long)metrics[i].lake_hash,
                (unsigned long long)metrics[i].ocean_hash,
                (unsigned long long)metrics[i].bay_hash,
                (unsigned long long)metrics[i].shallow_hash, zooms[i]);
    }
    semantics_after = water_semantics_hash(snapshot);
    contract_ok &= semantics_before == semantics_after;
    identity_ok = water_ready_identity_contract(
        snapshot, &ready_current, &ready_stale, &ready_restored);
    geometry_ok = game_presentation_water_reference_probe(
        canvas, &pattern_matches, &exact_matches, &pattern_pixels,
        &raw_unique, &pattern_max_delta);
    legend_ok = game_presentation_water_legend_probe(summary, canvas);
    fprintf(summary,
            "case=static_lake_ocean_contract ok=%d lakes=%d oceans=%d "
            "bays=%d shallow_sea=%d semantics=%llu/%llu motifs_excluded=%d\n",
            contract_ok, lake_count, ocean_count, bay_count, shallow_count,
            (unsigned long long)semantics_before,
            (unsigned long long)semantics_after, lake_motifs_excluded);
    fprintf(summary,
            "case=static_water_surface_identity ok=%d current=%d "
            "stale_rejected=%d restored=%d\n",
            identity_ok, ready_current, ready_stale, ready_restored);
    fprintf(summary,
            "case=static_lake_wave_geometry ok=%d tint=%d geometry=%d "
            "pattern_frames=%d raw_unique=%d matches=%d/%d "
            "exact=%d/%d max_channel_delta=%d\n",
            tint_ok && geometry_ok && pattern_frames > 0, tint_ok,
            geometry_ok, pattern_frames, raw_unique, pattern_matches,
            pattern_pixels, exact_matches, pattern_pixels,
            pattern_max_delta);
    fprintf(summary,
            "case=static_coast_coverage_locality ok=%d samples=%d leaks=%d "
            "lake_samples=%d lake_leaks=%d lake_category_mismatches=%d "
            "stats=%d hash=%llu/%llu removed=%llu filled=%llu "
            "protected=%llu\n",
            coverage_ok, coverage.samples, coverage.leaks,
            coverage.lake_samples, coverage.lake_leaks,
            coverage.category_lake_mismatches, coverage.stats_consistent,
            (unsigned long long)coverage.shared_hash,
            (unsigned long long)coverage.cached_hash,
            (unsigned long long)coverage.removed_ocean_tiles,
            (unsigned long long)coverage.filled_land_tiles,
            (unsigned long long)coverage.protected_land_tiles);
    restore_ui(old_ui);
    return contract_ok && tint_ok && identity_ok && geometry_ok && legend_ok &&
           pattern_frames > 0 &&
           coverage_ok && zoom_ok && artifact_ok;
}
