#include "render/render_static_map_cache.h"

#include "core/dirty_flags.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_keys.h"
#include "render/map_display_policy.h"
#include "render/coast_geometry.h"
#include "render/map_ownership_surface.h"
#include "render/render_context.h"
#include "render/river_render.h"
#include "render/render_static_map_cache_internal.h"
#include "render/render_static_map_surface.h"
#include "render/render_static_map_cache_status.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_water_surface_cache.h"
#include "render/snapshot_map_layers.h"
#include "render/wind_render.h"
#include <stdio.h>

static MapLayerCache fill_cache;
static MapLayerCache border_cache;
static MapLayerCache static_map_dynamic_cache;
static MapLayerCache static_map_geography_cache;
static MapLayerCache static_map_climate_cache;
static int cache_needs_work;
static int snapshot_fallback_draws;
static int static_map_compositions;

static MapLayerCache *static_cache_for_display(void) {
    if (display_mode == DISPLAY_GEOGRAPHY) return &static_map_geography_cache;
    if (display_mode == DISPLAY_CLIMATE) return &static_map_climate_cache;
    return &static_map_dynamic_cache;
}

static int combined_revision(int a, int b) { return (a * 1000003) ^ b; }
static void record_cache_step(DWORD start, ProfilerRenderSubphase subphase, const char *name) {
    profiler_record_render_subphase(subphase, PROFILER_SPIKE_STATIC_CACHE,
                                    name, (int)(GetTickCount() - start));
}

static int cache_w(void) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    return max(1, (snapshot ? snapshot->map_w : DEFAULT_MAP_W) * MAP_LAYER_CACHE_SCALE);
}

static int cache_h(void) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    return max(1, (snapshot ? snapshot->map_h : DEFAULT_MAP_H) * MAP_LAYER_CACHE_SCALE);
}

static int ensure_cache(HDC hdc, MapLayerCache *cache) {
    return render_static_map_surface_ensure(hdc, cache, cache_w(), cache_h());
}
static int cache_matches(const MapLayerCache *cache, int revision) {
    return render_static_map_surface_matches(cache, cache_w(), cache_h(), revision,
                                             display_mode);
}

static int cache_matches_display(const MapLayerCache *cache, int revision, int display_key) {
    return render_static_map_surface_matches(cache, cache_w(), cache_h(), revision,
                                             display_key);
}

static int border_display_key(void) {
    return display_mode == DISPLAY_REGIONS ? DISPLAY_REGIONS : DISPLAY_OVERVIEW;
}

static int border_cache_matches(int revision) {
    return cache_matches_display(&border_cache, revision, border_display_key());
}

static int cache_presentable(const MapLayerCache *cache) {
    return render_static_map_surface_presentable(cache, cache_w(), cache_h(), display_mode);
}

static void mark_cache_valid(MapLayerCache *cache, int revision, int complete) {
    render_static_map_surface_mark_valid(cache, revision, display_mode, complete);
}

static int display_requires_fill_layer(void) {
    return map_display_policy_requires_fill_layer(display_mode);
}

static int cache_stretch_mode(MapLayout layout) {
    if (display_mode == DISPLAY_GEOGRAPHY || display_mode == DISPLAY_CLIMATE) {
        /* GDI HALFTONE dithers adjacent land/water colors into alternating
           cyan and pale-land combs at non-integer zooms. Keep the immutable
           semantic base crisp; the separate 4x water surface owns shoreline
           alpha and texture smoothing. */
        (void)layout;
        return render_static_map_surface_categorical_stretch_mode();
    }
    if (auto_run && world_generated && speed_index >= SPEED_COUNT - 1) return COLORONCOLOR;
    return layout.tile_size <= 2 ? HALFTONE : COLORONCOLOR;
}

static void alpha_cache(HDC dst, const MapLayerCache *src) {
    render_static_map_surface_alpha(dst, src);
}

static int compose_static_map(HDC hdc, MapLayerCache *cache, int revision) {
    if (!ensure_cache(hdc, cache) ||
        cache->width != render_static_physical_cache_width() ||
        cache->height != render_static_physical_cache_height() ||
        !render_static_physical_cache_compose_base_coast(cache->dc,
                                                         display_mode)) return 0;
    if (display_requires_fill_layer() && fill_cache.valid)
        alpha_cache(cache->dc, &fill_cache);
    mark_cache_valid(cache, revision, 1);
    static_map_compositions++;
    return 1;
}

static void present_cache(HDC hdc, RECT client, MapLayout layout, const MapLayerCache *cache) {
    render_static_map_surface_present(hdc, client, layout, cache, cache_stretch_mode(layout));
}

static int physical_display_key(void) {
    return (int)map_display_policy_physical_family(display_mode);
}

static int physical_revision(int tile_key) { return combined_revision(tile_key, physical_display_key()); }

static int fill_revision(int ownership_key, int civ_key, int alliance_key) {
    int key = ownership_key;
    if (display_mode == DISPLAY_ALLIANCE) key = combined_revision(key, combined_revision(civ_key, alliance_key));
    else if (display_mode == DISPLAY_POLITICAL || display_mode == DISPLAY_ALL) key = combined_revision(key, civ_key);
    return combined_revision(key, display_mode);
}

static int border_revision(int tile_key, int region_key) { return combined_revision(tile_key, region_key); }

static int static_revision(int physical_key, int fill_key, int coast_key) {
    return combined_revision(combined_revision(physical_key, fill_key), coast_key);
}

static int presentation_revision(int static_key, int hydro_key, int border_key) {
    return combined_revision(static_key, combined_revision(hydro_key, border_key));
}

static void draw_blank(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    RECT map_rect = {layout.map_x, layout.map_y, layout.map_x + layout.draw_w, layout.map_y + layout.draw_h};
    if (!snapshot || !snapshot->world_generated) fill_rect(hdc, get_map_viewport_rect(client), RGB(79, 160, 215));
    fill_rect(hdc, map_rect, RGB(64, 133, 178));
}

static int static_layers_ready(int physical_ready) {
    return physical_ready && (!display_requires_fill_layer() || fill_cache.valid);
}

static int river_overlay_ready(const RenderSnapshot *snapshot, int lod) {
    if (!snapshot || !snapshot->rivers.valid ||
        snapshot->rivers.map_w != snapshot->map_w ||
        snapshot->rivers.map_h != snapshot->map_h) return 1;
    return render_static_physical_overlay_cache_river_ready(snapshot, lod);
}

static int wind_overlay_ready(const RenderSnapshot *snapshot, int lod) {
    if (!snapshot || !map_display_policy_shows_wind(display_mode) ||
        !snapshot->wind.valid || snapshot->wind.map_w != snapshot->map_w ||
        snapshot->wind.map_h != snapshot->map_h) return 1;
    return render_static_physical_overlay_cache_wind_ready(snapshot, lod);
}

static int static_cache_work_pending(const MapLayerCache *cache,
                                     int physical_ready, int fill_key,
                                     int border_key, int static_key) {
    return !physical_ready ||
           (display_requires_fill_layer() && !cache_matches(&fill_cache, fill_key)) ||
           !border_cache_matches(border_key) ||
           !cache_matches(cache, static_key);
}

static const MapLayerCache *best_present_cache(const MapLayerCache *cache,
                                               int static_key) {
    if (cache->complete && cache_matches(cache, static_key)) return cache;
    return NULL;
}

static int rebuild_fill_layer(HDC hdc, int fill_key, int live_fill_key) {
    DWORD start = GetTickCount();
    render_static_map_cache_status_note_reason(RENDER_STATIC_MAP_REASON_FILL);
    if (!ensure_cache(hdc, &fill_cache)) return 0;
    render_static_map_cache_build_fill_pixels(&fill_cache,
                                              render_context_snapshot());
    mark_cache_valid(&fill_cache, fill_key, 1);
    profiler_add_render_rebuild(PROFILER_RENDER_POLITICAL);
    record_cache_step(start, PROFILER_RENDER_SUB_FILL, "Political/province fill");
    if (fill_key == live_fill_key) dirty_clear_render_political();
    return 1;
}

static int rebuild_border_layer(HDC hdc, int border_key, int live_border_key) {
    DWORD step_start = GetTickCount();
    render_static_map_cache_status_note_reason(RENDER_STATIC_MAP_REASON_BORDER);
    if (ensure_cache(hdc, &border_cache)) {
        render_static_map_cache_build_border_pixels(&border_cache, render_context_snapshot());
        render_static_map_surface_mark_valid(&border_cache, border_key,
                                             border_display_key(), 1);
        profiler_add_render_rebuild(PROFILER_RENDER_BORDER);
        if (border_key == live_border_key) dirty_clear_render_borders();
    }
    record_cache_step(step_start, PROFILER_RENDER_SUB_BORDERS, "Borders/contours");
    return border_cache_matches(border_key);
}

static int hydrology_revision(int hydrology_key, int river_key, int wind_key,
                              int river_lod, int wind_lod) {
    int key = combined_revision(combined_revision(hydrology_key, river_key), river_lod);
    if (map_display_policy_shows_wind(display_mode)) {
        key = combined_revision(key, combined_revision(wind_key, wind_lod));
    }
    return key;
}

static void present_static_and_border(HDC hdc, RECT client, MapLayout layout,
                                      const MapLayerCache *cache,
    const RenderSnapshot *snapshot,
    int physical_ready, int river_lod) {
    present_cache(hdc, client, layout, cache);
    render_water_surface_cache_present_ocean(hdc, client, layout, snapshot);
    render_water_surface_cache_present_lake(hdc, client, layout, snapshot);
    if (physical_ready) {
        render_static_physical_overlay_cache_present_river(
            hdc, client, layout, snapshot, river_lod);
    }
    render_static_map_surface_present_transparent(hdc, client, layout, &border_cache,
                                                  cache_stretch_mode(layout),
                                                  MAP_TRANSPARENT_KEY);
}

static void draw_snapshot_fallback(HDC hdc, RECT client, MapLayout layout,
                                   const RenderSnapshot *snapshot, int river_lod) {
    draw_snapshot_terrain_layer(hdc, client, layout);
    render_water_surface_cache_present_ocean(hdc, client, layout, snapshot);
    render_water_surface_cache_present_lake(hdc, client, layout, snapshot);
    if (render_static_physical_overlay_cache_river_ready(snapshot, river_lod)) {
        render_static_physical_overlay_cache_present_river(
            hdc, client, layout, snapshot, river_lod);
    }
    draw_snapshot_border_layer(hdc, client, layout);
    snapshot_fallback_draws++;
}

void draw_cached_static_map_nonblocking(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    MapLayerCache *static_cache = static_cache_for_display();
    int physical_key, fill_key, coast_key, hydro_key, border_key;
    int ownership_key, live_ownership_key, live_tile_key, live_region_key, live_civ_key;
    int live_physical_key, live_fill_key, live_coast_key, live_hydro_key, live_border_key;
    int live_static_key, target_static_key;
    int target_present_key, live_present_key;
    int target_fill_key;
    int fill_needs, border_needs, physical_ready;
    int river_lod = river_render_lod_bucket_for_zoom(map_zoom_percent,
                                                     layout.tile_size);
    int wind_lod = wind_render_lod_bucket_for_tile_size(layout.tile_size);
    const MapLayerCache *present;
    if (!snapshot || !snapshot->world_generated) {
        cache_needs_work = 0;
        render_static_map_cache_status_reset_keys();
        render_static_map_cache_status_set_presented(0, 0, 0, 0);
        if (static_cache->complete && cache_presentable(static_cache)) {
            render_static_map_cache_status_set_presented(0, 1, 1, 0);
            present_cache(hdc, client, layout, static_cache);
        } else draw_blank(hdc, client, layout);
        return;
    }
    physical_key = physical_revision(snapshot->terrain_revision);
    ownership_key = map_ownership_surface_snapshot_revision(snapshot);
    fill_key = fill_revision(ownership_key, snapshot->civ_visual_revision, snapshot->alliance_revision);
    coast_key = snapshot->coast_revision;
    hydro_key = hydrology_revision(snapshot->hydrology_revision, snapshot->river_revision,
                                   snapshot->wind_revision, river_lod, wind_lod);
    border_key = border_revision(snapshot->terrain_revision, snapshot->regions_revision);
    live_tile_key = dirty_revision_terrain();
    live_region_key = render_snapshot_regions_revision_key();
    live_civ_key = render_snapshot_civ_visual_revision_key();
    live_ownership_key = map_ownership_surface_live_revision();
    live_physical_key = physical_revision(live_tile_key);
    live_fill_key = fill_revision(live_ownership_key, live_civ_key, dirty_revision_alliance());
    live_coast_key = dirty_revision_coast();
    live_hydro_key = hydrology_revision(dirty_revision_hydrology(),
                                        render_snapshot_river_revision_key(),
                                        render_snapshot_wind_revision_key(),
                                        river_lod, wind_lod);
    live_border_key = border_revision(live_tile_key, live_region_key);
    target_fill_key = display_requires_fill_layer() ?
                      fill_key : 0;
    live_static_key = static_revision(live_physical_key,
                                      display_requires_fill_layer() ? live_fill_key : 0,
                                      live_coast_key);
    target_static_key = static_revision(physical_key, target_fill_key, coast_key);
    target_present_key = presentation_revision(target_static_key, hydro_key, border_key);
    live_present_key = presentation_revision(live_static_key, live_hydro_key, live_border_key);
    render_static_map_cache_status_set_keys(
        target_present_key, live_present_key, target_fill_key, border_key,
        display_requires_fill_layer() ? live_fill_key : 0, live_border_key);
    physical_ready = render_static_physical_cache_selected_ready(
                         snapshot, display_mode, river_lod, wind_lod) &&
                     river_overlay_ready(snapshot, river_lod) &&
                     wind_overlay_ready(snapshot, wind_lod);
    if (!physical_ready)
        render_static_map_cache_status_note_reason(
            RENDER_STATIC_MAP_REASON_PHYSICAL);
    if (physical_ready) {
        if (physical_key == live_physical_key) dirty_clear_render_terrain();
        if (coast_key == live_coast_key) dirty_clear_render_coast();
        if (hydro_key == live_hydro_key) dirty_clear_render_hydrology();
    }

    if (map_interaction_preview && static_cache->complete &&
        cache_matches(static_cache, target_static_key) &&
        border_cache_matches(border_key)) {
        int current = cache_matches(static_cache, target_static_key) && physical_ready;
        int ownership_current = !display_requires_fill_layer() || current;
        cache_needs_work = 0;
        render_static_map_cache_status_set_presented(current, static_cache->complete,
                                                     current, ownership_current);
        render_static_map_cache_status_note_published(target_fill_key, border_key, ownership_current);
        present_static_and_border(hdc, client, layout, static_cache, snapshot,
                                  physical_ready, river_lod);
        return;
    }
    fill_needs = display_requires_fill_layer() &&
                  !cache_matches(&fill_cache, target_fill_key);
    border_needs = !border_cache_matches(border_key) ||
                   (dirty_render_borders() && border_key == live_border_key);

    if (fill_needs && physical_ready &&
        rebuild_fill_layer(hdc, target_fill_key, live_fill_key)) {
        fill_needs = 0;
        if (border_needs && cache_matches(&fill_cache, target_fill_key)) {
            border_needs = !rebuild_border_layer(hdc, border_key, live_border_key);
        }
    }
    if (physical_ready && border_needs &&
        (!display_requires_fill_layer() ||
         cache_matches(&fill_cache, target_fill_key))) {
        rebuild_border_layer(hdc, border_key, live_border_key);
    }
    if (static_layers_ready(physical_ready) &&
        !cache_matches(static_cache, target_static_key)) {
        DWORD step_start = GetTickCount();
        compose_static_map(hdc, static_cache, target_static_key);
        record_cache_step(step_start, PROFILER_RENDER_SUB_STATIC_CACHE, "Static map compose");
    }

    present = best_present_cache(static_cache, target_static_key);
    if (present && border_cache_matches(border_key)) {
        int static_current = present == static_cache &&
                             cache_matches(static_cache, target_static_key);
        int current = static_current && physical_ready;
        int safe = present->complete && (!display_requires_fill_layer() || static_current);
        int full = present->complete && current;
        int ownership_current = safe && (!display_requires_fill_layer() || static_current);
        render_static_map_cache_status_set_presented(current, safe, full, ownership_current);
        render_static_map_cache_status_note_published(target_fill_key, border_key, ownership_current);
        present_static_and_border(hdc, client, layout, present, snapshot,
                                  physical_ready, river_lod);
    } else {
        draw_snapshot_fallback(hdc, client, layout, snapshot, river_lod);
        render_static_map_cache_status_set_presented(1, 1, 0, 1);
        render_static_map_cache_status_note_published(target_fill_key,
                                                      border_key, 1);
    }
    cache_needs_work = physical_ready && static_cache_work_pending(
        static_cache, physical_ready, target_fill_key, border_key,
        target_static_key);
}

int render_static_map_cache_needs_work(void) { return cache_needs_work; }
int render_static_map_cache_snapshot_fallback_draws(void) { return snapshot_fallback_draws; }
int render_static_map_cache_compositions(void) { return static_map_compositions; }

static RenderLayerCacheMemory map_surface_memory(const MapLayerCache *cache) {
    RenderLayerCacheMemory memory = {0};
    if (!cache) return memory;
    if (cache->bitmap) {
        memory.bitmaps = 1;
        memory.bitmap_bytes =
            (uint64_t)max(0, cache->width) * (uint64_t)max(0, cache->height) * 4u;
    }
    if (cache->dc) memory.dcs = 1;
    return memory;
}

RenderLayerCacheMemory render_static_map_cache_memory(void) {
    RenderLayerCacheMemory total = {0};
    render_layer_cache_memory_add(&total, map_surface_memory(&fill_cache));
    render_layer_cache_memory_add(&total, map_surface_memory(&border_cache));
    render_layer_cache_memory_add(&total, map_surface_memory(&static_map_dynamic_cache));
    render_layer_cache_memory_add(&total, map_surface_memory(&static_map_geography_cache));
    render_layer_cache_memory_add(&total, map_surface_memory(&static_map_climate_cache));
    return total;
}

void render_static_map_cache_invalidate_all(void) {
    render_static_map_surface_release(&fill_cache);
    render_static_map_surface_release(&border_cache);
    render_static_map_surface_release(&static_map_dynamic_cache);
    render_static_map_surface_release(&static_map_geography_cache);
    render_static_map_surface_release(&static_map_climate_cache);
    render_static_physical_cache_invalidate();
    render_static_physical_overlay_cache_invalidate();
    cache_needs_work = 0;
    static_map_compositions = 0;
    render_static_map_cache_status_reset_keys();
    render_static_map_cache_status_set_presented(0, 0, 0, 0);
}

void render_static_map_cache_reset_debug(void) { render_static_map_cache_invalidate_all(); }
