#include "game/game_worldgen_failure_render_probe.h"

#include "core/render_snapshot.h"
#include "core/render_snapshot_river.h"
#include "core/worldgen_fault_injection.h"
#include "render/render_allocation_diagnostics.h"
#include "render/render_context.h"
#include "render/map_display_policy.h"
#include "render/render_ocean_decoration.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_static_scene.h"
#include "render/render_water_coverage.h"
#include "render/render_water_surface_cache.h"
#include "render/river_geometry.h"
#include "render/river_render.h"
#include "render/river_topology.h"
#include "ui/ui_layout.h"
#include "ui/ui_types.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { FALLBACK_W = 640, FALLBACK_H = 360 };

static uint64_t allocation_attempts(const RenderAllocationDiagnostics *value) {
    uint64_t total = 0;
    int owner;
    for (owner = 0; owner < RENDER_ALLOCATION_COUNT; owner++)
        total += value->attempts[owner];
    return total;
}

static int allocation_initial_ocean_ready(
    const RenderAllocationDiagnostics *before,
    const RenderAllocationDiagnostics *after) {
    int owner;
    for (owner = 0; owner < RENDER_ALLOCATION_COUNT; owner++) {
        uint64_t expected = owner == RENDER_ALLOCATION_LAYER_CACHE ? 3u : 0u;
        if (after->attempts[owner] != before->attempts[owner] + expected ||
            after->failures[owner] != before->failures[owner]) return 0;
    }
    return after->injected_failures == before->injected_failures;
}

static int allocation_settled_equal(
    const RenderAllocationDiagnostics *before,
    const RenderAllocationDiagnostics *after) {
    return memcmp(before->attempts, after->attempts,
                  sizeof(before->attempts)) == 0 &&
           memcmp(before->failures, after->failures,
                  sizeof(before->failures)) == 0 &&
           before->injected_failures == after->injected_failures;
}

static int river_overlay_failure_retry_probe(HDC hdc, FILE *file) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    RenderStaticPhysicalOverlayCacheStats before = {0};
    RenderStaticPhysicalOverlayCacheStats after_failure = {0};
    RenderStaticPhysicalOverlayCacheStats after_retry = {0};
    RECT client = {0, 0, 32, 24};
    MapLayout layout = {0, 0, 2, 32, 24};
    int malformed_rejected = 0;
    int retry_ready = 0;
    int lod_failure_rejected = 0;
    int direct_retry = 0;
    int storage_reused = 0;
    int ok = 0;
    int i;
    if (!snapshot || !hdc) goto cleanup;
    snapshot->world_generated = 1;
    snapshot->map_w = 16;
    snapshot->map_h = 12;
    snapshot->terrain_revision = 910001;
    snapshot->hydrology_revision = 910002;
    snapshot->river_revision = 910003;
    for (i = 0; i < snapshot->map_w * snapshot->map_h; i++) {
        snapshot->tiles[i].geography = GEO_PLAIN;
        snapshot->tiles[i].climate = CLIMATE_TEMPERATE_MONSOON;
        snapshot->tiles[i].elevation = 80;
    }
    snapshot->rivers.valid = 1;
    snapshot->rivers.revision = snapshot->river_revision;
    snapshot->rivers.map_w = snapshot->map_w;
    snapshot->rivers.map_h = snapshot->map_h;
    snapshot->rivers.path_count = 1;
    snapshot->rivers.capacity = 1;
    snapshot->rivers.paths = NULL;
    render_static_physical_overlay_cache_invalidate();
    before = *render_static_physical_overlay_cache_stats();
    malformed_rejected =
        !render_static_physical_overlay_cache_ensure_river(hdc, snapshot, 0) &&
        !render_static_physical_overlay_cache_river_ready(snapshot, 0);
    after_failure = *render_static_physical_overlay_cache_stats();
    snapshot->rivers.path_count = 0;
    snapshot->rivers.capacity = 0;
    if (!render_snapshot_river_reserve(&snapshot->rivers, 1)) goto cleanup;
    snapshot->rivers.paths[0].point_count = 3;
    snapshot->rivers.paths[0].order = 4;
    snapshot->rivers.paths[0].flow = 1200;
    snapshot->rivers.paths[0].terminal_inflow = 1200;
    snapshot->rivers.paths[0].width = 5;
    snapshot->rivers.paths[0].points[0].x = 2;
    snapshot->rivers.paths[0].points[0].y = 2;
    snapshot->rivers.paths[0].points[1].x = 7;
    snapshot->rivers.paths[0].points[1].y = 5;
    snapshot->rivers.paths[0].points[2].x = 12;
    snapshot->rivers.paths[0].points[2].y = 8;
    snapshot->rivers.path_count = 1;
    snapshot->rivers.valid = 1;
    retry_ready = render_static_physical_overlay_cache_ensure_river(
                      hdc, snapshot, 0) &&
                  render_static_physical_overlay_cache_river_ready(snapshot, 0);
    after_retry = *render_static_physical_overlay_cache_stats();
    storage_reused = after_retry.river_surface_allocations ==
                         before.river_surface_allocations + 1 &&
                     after_retry.river_rebuilds == before.river_rebuilds + 1;
    river_geometry_rebuild(snapshot);
    river_topology_release();
    lod_failure_rejected =
        !river_render_draw_layer_lod(hdc, client, layout, snapshot, 0);
    river_geometry_release();
    direct_retry = river_render_draw_layer_lod(
        hdc, client, layout, snapshot, 0);
    ok = malformed_rejected && retry_ready && storage_reused &&
         lod_failure_rejected && direct_retry &&
         after_failure.river_build_failures ==
             before.river_build_failures + 1 &&
         after_retry.river_build_failures ==
             after_failure.river_build_failures;
cleanup:
    if (file) {
        fprintf(file,
                "case=river_overlay_prepare_failure_retry malformed_rejected=%d "
                "lod_failure_rejected=%d retry_ready=%d direct_retry=%d "
                "surface_reused=%d failures=%d/%d/%d result=%s\n",
                malformed_rejected, lod_failure_rejected, retry_ready,
                direct_retry, storage_reused, before.river_build_failures,
                after_failure.river_build_failures,
                after_retry.river_build_failures, ok ? "PASS" : "FAIL");
    }
    render_static_physical_overlay_cache_invalidate();
    if (snapshot) {
        render_snapshot_river_release(&snapshot->rivers);
        free(snapshot);
    }
    return ok;
}

static int count_nonblank(const uint32_t *pixels) {
    int count = 0;
    int i;
    for (i = 0; i < FALLBACK_W * FALLBACK_H; i++)
        count += (pixels[i] & UINT32_C(0x00ffffff)) != 0;
    return count;
}

static uint32_t dib_rgb(unsigned int red, unsigned int green,
                        unsigned int blue) {
    return (red << 16) | (green << 8) | blue;
}

static void count_rendered_categories(const uint32_t *pixels,
                                      const RenderSnapshot *snapshot,
                                      MapLayout layout, int *land,
                                      int *ocean, int *lake) {
    RECT map_rect = {layout.map_x, layout.map_y,
                     layout.map_x + layout.draw_w,
                     layout.map_y + layout.draw_h};
    RECT client = {0, 0, FALLBACK_W, FALLBACK_H};
    RECT visible;
    int x, y;
    *land = *ocean = *lake = 0;
    if (!IntersectRect(&visible, &map_rect, &client)) return;
    for (y = visible.top; y < visible.bottom; y++) {
        int tile_y = clamp((int)((long long)(y - layout.map_y) *
                         snapshot->map_h / max(1, layout.draw_h)),
                         0, snapshot->map_h - 1);
        for (x = visible.left; x < visible.right; x++) {
            uint32_t color = pixels[y * FALLBACK_W + x] &
                             UINT32_C(0x00ffffff);
            int tile_x;
            int geography;
            if (!color) continue;
            tile_x = clamp((int)((long long)(x - layout.map_x) *
                           snapshot->map_w / max(1, layout.draw_w)),
                           0, snapshot->map_w - 1);
            geography = snapshot->tiles[
                tile_y * snapshot->map_w + tile_x].geography;
            if (geography == GEO_LAKE) (*lake)++;
            else if (geography == GEO_OCEAN || geography == GEO_BAY) (*ocean)++;
            else (*land)++;
        }
    }
}

static void count_visible_categories(const RenderSnapshot *snapshot,
                                     MapLayout layout, int *land,
                                     int *ocean, int *lake) {
    RECT map_rect = {layout.map_x, layout.map_y,
                     layout.map_x + layout.draw_w,
                     layout.map_y + layout.draw_h};
    RECT client = {0, 0, FALLBACK_W, FALLBACK_H};
    RECT visible;
    int x, y;
    *land = *ocean = *lake = 0;
    if (!IntersectRect(&visible, &map_rect, &client)) return;
    for (y = visible.top; y < visible.bottom; y++) {
        int tile_y = clamp((int)((long long)(y - layout.map_y) *
                         snapshot->map_h / max(1, layout.draw_h)),
                         0, snapshot->map_h - 1);
        for (x = visible.left; x < visible.right; x++) {
            int tile_x = clamp((int)((long long)(x - layout.map_x) *
                             snapshot->map_w / max(1, layout.draw_w)),
                             0, snapshot->map_w - 1);
            int geography = snapshot->tiles[
                tile_y * snapshot->map_w + tile_x].geography;
            if (geography == GEO_LAKE) (*lake)++;
            else if (geography == GEO_OCEAN || geography == GEO_BAY) (*ocean)++;
            else (*land)++;
        }
    }
}

static int count_land_matches(const uint32_t *pixels,
                              const RenderSnapshot *snapshot,
                              MapLayout layout, int mode) {
    RECT map_rect = {layout.map_x, layout.map_y,
                     layout.map_x + layout.draw_w,
                     layout.map_y + layout.draw_h};
    RECT client = {0, 0, FALLBACK_W, FALLBACK_H};
    RECT visible;
    int matches = 0;
    int x, y;
    if (!IntersectRect(&visible, &map_rect, &client)) return 0;
    for (y = visible.top; y < visible.bottom; y++) {
        int tile_y = clamp((int)((long long)(y - layout.map_y) *
                         snapshot->map_h / max(1, layout.draw_h)),
                         0, snapshot->map_h - 1);
        for (x = visible.left; x < visible.right; x++) {
            int tile_x = clamp((int)((long long)(x - layout.map_x) *
                             snapshot->map_w / max(1, layout.draw_w)),
                             0, snapshot->map_w - 1);
            const SnapshotTile *tile = &snapshot->tiles[
                tile_y * snapshot->map_w + tile_x];
            COLORREF expected;
            if (tile->geography == GEO_LAKE || tile->geography == GEO_OCEAN ||
                tile->geography == GEO_BAY) continue;
            expected = map_display_policy_snapshot_tile_color(snapshot, tile,
                                                               mode);
            matches += (pixels[y * FALLBACK_W + x] &
                        UINT32_C(0x00ffffff)) ==
                       dib_rgb(GetRValue(expected), GetGValue(expected),
                               GetBValue(expected));
        }
    }
    return matches;
}

int game_worldgen_failure_render_probe(FILE *file) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    BITMAPINFO info;
    RECT client = {0, 0, FALLBACK_W, FALLBACK_H};
    HDC screen = GetDC(NULL);
    HDC memory = screen ? CreateCompatibleDC(screen) : NULL;
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    uint32_t *pixels = NULL;
    MapLayout layout;
    RenderAllocationDiagnostics before_alloc = {0}, mid_alloc = {0};
    RenderAllocationDiagnostics after_alloc = {0};
    RenderStaticPhysicalCacheStats before_physical = {0};
    RenderStaticPhysicalOverlayCacheStats before_overlay = {0};
    RenderWaterSurfaceCacheStats before_water = {0}, after_water = {0};
    RenderWaterCoverageStats before_coverage = {0};
    int old_mode = display_mode;
    int old_zoom = map_zoom_percent;
    int old_preview = map_interaction_preview;
    int fallback_before = render_static_map_cache_snapshot_fallback_draws();
    int compositions_before;
    int first_nonblank = 0, second_nonblank = 0;
    int first_land = 0, first_ocean = 0, first_lake = 0;
    int second_land = 0, second_ocean = 0, second_lake = 0;
    int visible_land = 0, visible_ocean = 0, visible_lake = 0;
    int geography_matches = 0, climate_matches = 0;
    int immutable_ok = 0, scene_ok = 0, ok = 0;
    int river_failure_retry_ok = 0;

    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = FALLBACK_W;
    info.bmiHeader.biHeight = -FALLBACK_H;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    if (screen) bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                                          (void **)&pixels, NULL, 0);
    if (!snapshot || !snapshot->world_generated || !memory || !bitmap || !pixels)
        goto cleanup;
    old_bitmap = SelectObject(memory, bitmap);
    if (!old_bitmap || old_bitmap == HGDI_ERROR) {
        old_bitmap = NULL;
        goto cleanup;
    }
    river_failure_retry_ok = river_overlay_failure_retry_probe(memory, file);
    render_static_map_cache_invalidate_all();
    render_static_physical_cache_invalidate();
    render_static_physical_overlay_cache_invalidate();
    render_ocean_decoration_reset_debug();
    render_static_scene_invalidate_cache();
    compositions_before = render_static_map_cache_compositions();
    render_allocation_diagnostics_reset();
    worldgen_fault_injection_clear();
    before_physical = *render_static_physical_cache_stats();
    before_overlay = *render_static_physical_overlay_cache_stats();
    before_water = *render_water_surface_cache_stats();
    before_coverage = *render_water_coverage_stats();
    render_allocation_diagnostics_get(&before_alloc);
    display_mode = DISPLAY_GEOGRAPHY;
    map_zoom_percent = 100;
    map_interaction_preview = 0;
    layout = get_map_layout(client);
    memset(pixels, 0, FALLBACK_W * FALLBACK_H * sizeof(*pixels));
    render_context_begin(snapshot);
    render_static_scene_draw(memory, client, layout, snapshot);
    render_context_end();
    first_nonblank = count_nonblank(pixels);
    count_rendered_categories(pixels, snapshot, layout,
                              &first_land, &first_ocean, &first_lake);
    geography_matches = count_land_matches(
        pixels, snapshot, layout, DISPLAY_GEOGRAPHY);
    render_allocation_diagnostics_get(&mid_alloc);
    display_mode = DISPLAY_CLIMATE;
    memset(pixels, 0, FALLBACK_W * FALLBACK_H * sizeof(*pixels));
    render_context_begin(snapshot);
    render_static_scene_draw(memory, client, layout, snapshot);
    render_context_end();
    second_nonblank = count_nonblank(pixels);
    count_rendered_categories(pixels, snapshot, layout,
                              &second_land, &second_ocean, &second_lake);
    climate_matches = count_land_matches(
        pixels, snapshot, layout, DISPLAY_CLIMATE);
    count_visible_categories(snapshot, layout, &visible_land, &visible_ocean,
                             &visible_lake);
    render_allocation_diagnostics_get(&after_alloc);
    after_water = *render_water_surface_cache_stats();
    immutable_ok = allocation_initial_ocean_ready(&before_alloc, &mid_alloc) &&
        allocation_settled_equal(&mid_alloc, &after_alloc) &&
        render_static_physical_cache_stats()->base_rebuilds[0] ==
            before_physical.base_rebuilds[0] &&
        render_static_physical_cache_stats()->base_rebuilds[1] ==
            before_physical.base_rebuilds[1] &&
        render_static_physical_cache_stats()->base_rebuilds[2] ==
            before_physical.base_rebuilds[2] &&
        render_static_physical_cache_stats()->coast_rebuilds ==
            before_physical.coast_rebuilds &&
        render_static_physical_overlay_cache_stats()->river_ensure_calls ==
            before_overlay.river_ensure_calls &&
        render_static_physical_overlay_cache_stats()->river_rebuilds ==
            before_overlay.river_rebuilds &&
        render_static_physical_overlay_cache_stats()->river_path_visits ==
            before_overlay.river_path_visits &&
        render_static_physical_overlay_cache_stats()->wind_ensure_calls ==
            before_overlay.wind_ensure_calls &&
        render_static_physical_overlay_cache_stats()->wind_rebuilds ==
            before_overlay.wind_rebuilds &&
        render_static_physical_overlay_cache_stats()->wind_sample_visits ==
            before_overlay.wind_sample_visits &&
        after_water.rebuilds == before_water.rebuilds &&
        after_water.mask_pixel_scans == before_water.mask_pixel_scans &&
        render_water_coverage_stats()->rebuilds == before_coverage.rebuilds &&
        render_water_coverage_stats()->source_scans ==
            before_coverage.source_scans &&
        render_water_coverage_stats()->raster_samples ==
            before_coverage.raster_samples &&
        render_static_map_cache_compositions() == compositions_before;
    scene_ok = render_static_scene_presented_current() &&
        render_static_scene_presentable() && render_static_scene_complete() &&
        !render_static_scene_fully_current() &&
        !render_static_map_cache_needs_work();
    ok = first_nonblank > 0 && second_nonblank > 0 &&
        visible_land > 0 && visible_ocean > 0 && first_land > 0 &&
        first_ocean > 0 && second_land > 0 && second_ocean > 0 &&
        geography_matches > 100 && climate_matches > 100 &&
        (!visible_lake || (first_lake > 0 && second_lake > 0)) &&
        immutable_ok && scene_ok && river_failure_retry_ok &&
        render_static_map_cache_snapshot_fallback_draws() - fallback_before == 2 &&
        after_water.fallback_lake_draws == before_water.fallback_lake_draws &&
        after_water.fallback_ocean_draws == before_water.fallback_ocean_draws &&
        after_water.fallback_tile_scans == before_water.fallback_tile_scans;
cleanup:
    if (file) {
        fprintf(file,
                "worldgen_full_scene_fallback nonblank=%d/%d map=%d/%d/%d:%d/%d/%d expected=%d/%d/%d matches=%d/%d static=%d water=%d/%d "
                "water_scans=%llu initial_alloc_delta=%llu "
                "settled_alloc_delta=%llu immutable=%d current=%d safe=%d "
                "full=%d work=%d result=%s\n",
                first_nonblank, second_nonblank,
                first_land, first_ocean, first_lake,
                second_land, second_ocean, second_lake,
                visible_land, visible_ocean, visible_lake,
                geography_matches, climate_matches,
                render_static_map_cache_snapshot_fallback_draws() - fallback_before,
                after_water.fallback_lake_draws - before_water.fallback_lake_draws,
                after_water.fallback_ocean_draws - before_water.fallback_ocean_draws,
                (unsigned long long)(after_water.fallback_tile_scans -
                                     before_water.fallback_tile_scans),
                (unsigned long long)(allocation_attempts(&mid_alloc) -
                                     allocation_attempts(&before_alloc)),
                (unsigned long long)(allocation_attempts(&after_alloc) -
                                     allocation_attempts(&mid_alloc)),
                immutable_ok, render_static_scene_presented_current(),
                render_static_scene_presentable(),
                render_static_scene_fully_current(),
                render_static_map_cache_needs_work(), ok ? "PASS" : "FAIL");
    }
    display_mode = old_mode;
    map_zoom_percent = old_zoom;
    map_interaction_preview = old_preview;
    worldgen_fault_injection_clear();
    if (old_bitmap) SelectObject(memory, old_bitmap);
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
    if (screen) ReleaseDC(NULL, screen);
    if (snapshot) render_snapshot_release(snapshot);
    return ok;
}
