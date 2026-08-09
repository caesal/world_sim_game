#include "game/game_presentation_static_camera_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include "render/coast_geometry.h"
#include "render/render_allocation_diagnostics.h"
#include "render/render_context.h"
#include "render/render_ocean_assets.h"
#include "render/render_ocean_decoration.h"
#include "render/render_ocean_static_mask.h"
#include "render/render_ocean_texture.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_static_scene.h"
#include "render/render_water_coverage.h"
#include "render/render_water_surface_cache.h"
#include "render/river_geometry.h"
#include "render/river_render.h"
#include "render/wind_render.h"
#include "ui/ui_layout.h"

#include <string.h>

enum { CAMERA_PROBE_W = 1152, CAMERA_PROBE_H = 800 };

typedef struct {
    RenderStaticPhysicalCacheStats physical;
    RenderStaticPhysicalOverlayCacheStats overlay;
    CoastGeometryStats coast;
    RenderWaterSurfaceCacheStats water;
    RenderWaterCoverageStats coverage;
    OceanDecorationProbeInfo ocean;
    OceanDecorationDebugStats ocean_decoration;
    OceanTextureDebugStats ocean_texture;
    OceanAssetDebugStats ocean_assets;
    RenderOceanStaticMaskStats ocean_mask;
    WindRenderStats wind;
    HydrologyRenderStats river;
    RenderLayerCacheMemory map_memory;
    RenderLayerCacheMemory scene_memory;
    RenderLayerCacheMemory ocean_memory;
    RenderLayerCacheMemory water_memory;
    RenderAllocationDiagnostics allocation;
    int viewport_rebuilds;
    int fallback_draws;
    int static_compositions;
} CameraStamp;

typedef struct {
    DWORD gdi_objects;
    SIZE_T working_set;
    SIZE_T private_bytes;
    int valid;
} CameraResources;

typedef BOOL(WINAPI *MemoryInfoFn)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);

static void take_stamp(CameraStamp *stamp) {
    memset(stamp, 0, sizeof(*stamp));
    stamp->physical = *render_static_physical_cache_stats();
    stamp->overlay = *render_static_physical_overlay_cache_stats();
    stamp->coast = *coast_geometry_stats();
    stamp->water = *render_water_surface_cache_stats();
    stamp->coverage = *render_water_coverage_stats();
    stamp->ocean = render_ocean_decoration_probe_info();
    stamp->ocean_decoration = ocean_decoration_debug_stats;
    stamp->ocean_texture = render_ocean_texture_debug_stats();
    stamp->ocean_assets = ocean_assets_debug_stats();
    stamp->ocean_mask = *render_ocean_static_mask_stats();
    stamp->wind = *wind_render_stats();
    stamp->river = *river_geometry_stats();
    stamp->map_memory = render_static_map_cache_memory();
    stamp->scene_memory = render_static_scene_memory();
    stamp->ocean_memory = render_ocean_decoration_memory();
    stamp->water_memory = render_water_surface_cache_memory();
    render_allocation_diagnostics_get(&stamp->allocation);
    stamp->viewport_rebuilds = render_scene_cache_viewport_rebuilds();
    stamp->fallback_draws = render_static_map_cache_snapshot_fallback_draws();
    stamp->static_compositions = render_static_map_cache_compositions();
}

static int memory_equal(RenderLayerCacheMemory left,
                        RenderLayerCacheMemory right) {
    return left.bitmaps == right.bitmaps && left.dcs == right.dcs &&
           left.bitmap_bytes == right.bitmap_bytes;
}

static int allocation_equal(const RenderAllocationDiagnostics *left,
                            const RenderAllocationDiagnostics *right) {
    int owner;
    for (owner = 0; owner < RENDER_ALLOCATION_COUNT; owner++) {
        if (left->attempts[owner] != right->attempts[owner] ||
            left->failures[owner] != right->failures[owner]) return 0;
    }
    return left->injected_failures == right->injected_failures;
}

static int ocean_base_immutable_equal(const CameraStamp *left,
                                      const CameraStamp *right) {
    return left->ocean_texture.rebuilds ==
               right->ocean_texture.rebuilds &&
           left->ocean_texture.allocation_rebuilds ==
               right->ocean_texture.allocation_rebuilds &&
           left->ocean_texture.generation ==
               right->ocean_texture.generation &&
           left->ocean_texture.identity == right->ocean_texture.identity &&
           left->ocean_texture.source_identity ==
               right->ocean_texture.source_identity &&
           left->ocean_texture.width == right->ocean_texture.width &&
           left->ocean_texture.height == right->ocean_texture.height &&
           left->ocean_texture.tile_px == right->ocean_texture.tile_px &&
           left->ocean_texture.phase_x == right->ocean_texture.phase_x &&
           left->ocean_texture.phase_y == right->ocean_texture.phase_y &&
           left->ocean_texture.bitmap_identity ==
               right->ocean_texture.bitmap_identity &&
           left->ocean_texture.dc_identity ==
               right->ocean_texture.dc_identity &&
           left->ocean_texture.persistent_bytes ==
               right->ocean_texture.persistent_bytes &&
           left->ocean_assets.texture_decode_attempts ==
               right->ocean_assets.texture_decode_attempts &&
           left->ocean_assets.texture_raster_calls ==
               right->ocean_assets.texture_raster_calls &&
           left->ocean_assets.texture_tile_draw_calls ==
               right->ocean_assets.texture_tile_draw_calls &&
           left->ocean_assets.motif_decode_attempts ==
               right->ocean_assets.motif_decode_attempts &&
           left->ocean_assets.motif_draw_calls ==
               right->ocean_assets.motif_draw_calls &&
           left->ocean_decoration.composite_rebuilds ==
               right->ocean_decoration.composite_rebuilds &&
           left->ocean_decoration.exterior_layer_rebuilds ==
               right->ocean_decoration.exterior_layer_rebuilds &&
           left->ocean_decoration.interior_layer_rebuilds ==
               right->ocean_decoration.interior_layer_rebuilds &&
           left->ocean_decoration.exterior_layer_allocations ==
               right->ocean_decoration.exterior_layer_allocations &&
           left->ocean_decoration.interior_layer_allocations ==
               right->ocean_decoration.interior_layer_allocations &&
           left->ocean_decoration.exterior_layer_clears ==
               right->ocean_decoration.exterior_layer_clears &&
           left->ocean_decoration.interior_layer_clears ==
               right->ocean_decoration.interior_layer_clears &&
           left->ocean_decoration.exterior_layer_key ==
               right->ocean_decoration.exterior_layer_key &&
           left->ocean_decoration.interior_layer_key ==
               right->ocean_decoration.interior_layer_key &&
           left->ocean_mask.applications == right->ocean_mask.applications &&
           left->ocean_mask.pixel_scans == right->ocean_mask.pixel_scans;
}

static int immutable_equal(const CameraStamp *left,
                           const CameraStamp *right) {
    int family;
    for (family = 0; family < MAP_PHYSICAL_BASE_COUNT; family++) {
        if (left->physical.base_rebuilds[family] !=
                right->physical.base_rebuilds[family] ||
            left->physical.base_tile_scans[family] !=
                right->physical.base_tile_scans[family]) return 0;
    }
    return left->physical.coast_rebuilds == right->physical.coast_rebuilds &&
           left->physical.coast_tile_scans == right->physical.coast_tile_scans &&
           left->physical.persistent_bitmap_bytes ==
               right->physical.persistent_bitmap_bytes &&
           left->overlay.river_misses == right->overlay.river_misses &&
           left->overlay.river_rebuilds == right->overlay.river_rebuilds &&
           left->overlay.river_path_visits == right->overlay.river_path_visits &&
           left->overlay.wind_misses == right->overlay.wind_misses &&
           left->overlay.wind_rebuilds == right->overlay.wind_rebuilds &&
           left->overlay.wind_sample_visits == right->overlay.wind_sample_visits &&
           left->overlay.river_surface_allocations ==
               right->overlay.river_surface_allocations &&
           left->overlay.wind_surface_allocations ==
               right->overlay.wind_surface_allocations &&
           left->overlay.surface_clear_pixels ==
               right->overlay.surface_clear_pixels &&
           left->overlay.surface_finalize_pixels ==
               right->overlay.surface_finalize_pixels &&
           left->overlay.ready_river_mask == right->overlay.ready_river_mask &&
           left->overlay.ready_wind_mask == right->overlay.ready_wind_mask &&
           left->overlay.persistent_bitmap_bytes ==
               right->overlay.persistent_bitmap_bytes &&
           left->overlay.wind_anchor_bytes ==
               right->overlay.wind_anchor_bytes &&
           left->overlay.wind_sprite_bytes ==
               right->overlay.wind_sprite_bytes &&
           left->coast.rebuilds == right->coast.rebuilds &&
           left->coast.draws == right->coast.draws &&
           left->coast.tile_scans == right->coast.tile_scans &&
           left->coast.segment_visits == right->coast.segment_visits &&
           left->coast.mixed_cell_visits == right->coast.mixed_cell_visits &&
           left->water.rebuilds == right->water.rebuilds &&
           left->water.mask_pixel_scans == right->water.mask_pixel_scans &&
           left->water.fallback_lake_draws ==
               right->water.fallback_lake_draws &&
           left->water.fallback_ocean_draws ==
               right->water.fallback_ocean_draws &&
           left->water.fallback_tile_scans ==
               right->water.fallback_tile_scans &&
           left->water.persistent_bitmap_bytes ==
               right->water.persistent_bitmap_bytes &&
           left->coverage.rebuilds == right->coverage.rebuilds &&
           left->coverage.source_scans == right->coverage.source_scans &&
           left->coverage.raster_samples == right->coverage.raster_samples &&
           left->coverage.retained_bytes == right->coverage.retained_bytes &&
           left->ocean.item_rebuilds == right->ocean.item_rebuilds &&
           ocean_base_immutable_equal(left, right) &&
           left->wind.geometry_rebuild_count ==
               right->wind.geometry_rebuild_count &&
           left->wind.sprite_atlas_build_count ==
               right->wind.sprite_atlas_build_count &&
           left->wind.sprite_raster_count ==
               right->wind.sprite_raster_count &&
           left->wind.draw_count == right->wind.draw_count &&
           left->river.geometry_rebuild_count ==
               right->river.geometry_rebuild_count &&
           left->river.cache_rebuild_count == right->river.cache_rebuild_count &&
           memory_equal(left->map_memory, right->map_memory) &&
           memory_equal(left->scene_memory, right->scene_memory) &&
           memory_equal(left->ocean_memory, right->ocean_memory) &&
           memory_equal(left->water_memory, right->water_memory) &&
           allocation_equal(&left->allocation, &right->allocation) &&
           left->viewport_rebuilds == right->viewport_rebuilds &&
           left->fallback_draws == right->fallback_draws &&
           left->static_compositions == right->static_compositions;
}

static CameraResources process_resources(void) {
    CameraResources result = {0};
    PROCESS_MEMORY_COUNTERS_EX memory = {0};
    HMODULE module = GetModuleHandleA("kernel32.dll");
    MemoryInfoFn query = module ? (MemoryInfoFn)(void *)GetProcAddress(
                                      module, "K32GetProcessMemoryInfo") : NULL;
    memory.cb = sizeof(memory);
    result.gdi_objects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    if (query && query(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&memory,
                       sizeof(memory))) {
        result.working_set = memory.WorkingSetSize;
        result.private_bytes = memory.PrivateUsage;
        result.valid = result.gdi_objects > 0;
    }
    return result;
}

static CameraResources settle_process_resources(int *settled, int *samples) {
    CameraResources previous = process_resources();
    int i;
    *settled = 0;
    *samples = 1;
    for (i = 0; i < 8; i++) {
        CameraResources current = process_resources();
        (*samples)++;
        if (previous.valid && current.valid &&
            previous.gdi_objects == current.gdi_objects &&
            previous.private_bytes == current.private_bytes &&
            previous.working_set == current.working_set) {
            *settled = 1;
            return current;
        }
        previous = current;
    }
    return previous;
}

static int draw_camera(StaticPhysicalProbeCanvas *canvas,
                       const RenderSnapshot *snapshot, int mode, int zoom,
                       int offset_x, int offset_y, int centered) {
    RECT client = {0, 0, CAMERA_PROBE_W, CAMERA_PROBE_H};
    MapLayout layout;
    display_mode = mode;
    map_zoom_percent = zoom;
    map_offset_x = offset_x;
    map_offset_y = offset_y;
    map_view_auto_centered = centered;
    map_interaction_preview = 0;
    layout = get_map_layout(client);
    static_physical_probe_canvas_clear(canvas);
    render_context_begin(snapshot);
    render_static_scene_draw(canvas->dc, client, layout, snapshot);
    render_context_end();
    return render_static_scene_presentable();
}

static int run_zoom_sweep(StaticPhysicalProbeCanvas *canvas,
                          const RenderSnapshot *snapshot, int *steps) {
    int zoom;
    int ok = 1;
    *steps = 0;
    for (zoom = 25; zoom <= 700; zoom += 5) {
        ok &= draw_camera(canvas, snapshot, DISPLAY_GEOGRAPHY, zoom, 0, 0, 1);
        (*steps)++;
    }
    for (zoom = 695; zoom >= 25; zoom -= 5) {
        ok &= draw_camera(canvas, snapshot, DISPLAY_CLIMATE, zoom, 0, 0, 1);
        (*steps)++;
    }
    return ok;
}

static int run_pan_and_mode_cycles(StaticPhysicalProbeCanvas *canvas,
                                   const RenderSnapshot *snapshot,
                                   int cycles) {
    static const int zooms[] = {25, 50, 100, 200, 700};
    int i;
    int ok = 1;
    for (i = 0; i < cycles; i++) {
        int zoom = zooms[i % (int)(sizeof(zooms) / sizeof(zooms[0]))];
        int mode = (i & 1) ? DISPLAY_CLIMATE : DISPLAY_GEOGRAPHY;
        int offset_x = ((i * 37) % 241) - 120;
        int offset_y = ((i * 29) % 181) - 90;
        ok &= draw_camera(canvas, snapshot, mode, zoom,
                          offset_x, offset_y, 0);
    }
    return ok;
}

static int stale_revision_contract(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot) {
    RenderSnapshot *mutable_snapshot = (RenderSnapshot *)(uintptr_t)snapshot;
    CameraStamp before = {0}, stale = {0};
    int terrain_revision = snapshot->terrain_revision;
    int fallback_before = render_static_map_cache_snapshot_fallback_draws();
    int stale_draw, restored_draw, stale_state, no_rebuild, restored;
    int stale_current, stale_safe, stale_full;
    take_stamp(&before);
    mutable_snapshot->terrain_revision = terrain_revision + 101;
    stale_draw = draw_camera(canvas, snapshot, DISPLAY_GEOGRAPHY,
                             100, 0, 0, 1);
    take_stamp(&stale);
    stale_current = render_static_scene_presented_current();
    stale_safe = render_static_scene_presentable() &&
                 render_static_scene_complete();
    stale_full = render_static_scene_fully_current();
    stale_state = stale_draw && stale_current && stale_safe && !stale_full &&
        render_static_map_cache_snapshot_fallback_draws() - fallback_before == 1 &&
        !render_static_physical_cache_selected_ready(snapshot,
            DISPLAY_GEOGRAPHY, river_render_lod_bucket_for_zoom(100, 1),
            wind_render_lod_bucket_for_tile_size(1)) &&
        !render_water_surface_cache_ready(snapshot);
    no_rebuild = stale.physical.base_rebuilds[MAP_PHYSICAL_BASE_GEOGRAPHY] ==
                     before.physical.base_rebuilds[MAP_PHYSICAL_BASE_GEOGRAPHY] &&
        stale.physical.coast_rebuilds == before.physical.coast_rebuilds &&
        stale.overlay.river_rebuilds == before.overlay.river_rebuilds &&
        stale.overlay.wind_rebuilds == before.overlay.wind_rebuilds &&
        stale.water.rebuilds == before.water.rebuilds &&
        stale.coverage.rebuilds == before.coverage.rebuilds &&
        allocation_equal(&stale.allocation, &before.allocation) &&
        stale.static_compositions == before.static_compositions;
    mutable_snapshot->terrain_revision = terrain_revision;
    restored_draw = draw_camera(canvas, snapshot, DISPLAY_GEOGRAPHY,
                                100, 0, 0, 1);
    restored = restored_draw && render_static_scene_fully_current() &&
        render_static_physical_cache_selected_ready(snapshot,
            DISPLAY_GEOGRAPHY, river_render_lod_bucket_for_zoom(100, 1),
            wind_render_lod_bucket_for_tile_size(1)) &&
        render_water_surface_cache_ready(snapshot);
    fprintf(summary,
            "case=static_camera_stale_revision_rejected ok=%d stale_draw=%d "
            "fallback_delta=%d current=%d safe=%d full=%d no_rebuild=%d "
            "restored=%d\n",
            stale_state && no_rebuild && restored, stale_draw,
            render_static_map_cache_snapshot_fallback_draws() - fallback_before,
            stale_current, stale_safe, stale_full, no_rebuild, restored);
    return stale_state && no_rebuild && restored;
}

int game_presentation_static_camera_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot) {
    CameraStamp before = {0}, after_zoom = {0}, before_pan = {0}, after_pan = {0};
    CameraResources before_resources = {0}, after_resources = {0};
    int old_mode = display_mode;
    int old_zoom = map_zoom_percent;
    int old_x = map_offset_x;
    int old_y = map_offset_y;
    int old_centered = map_view_auto_centered;
    int old_preview = map_interaction_preview;
    int zoom_steps = 0;
    int sweep_draw_ok, sweep_cache_ok, warm_draw_ok, warm_cache_ok;
    int cycle_draw_ok, cycle_cache_ok;
    int resource_settled = 0, resource_samples = 0;
    int resource_ok, stale_ok, ok;
    if (!summary || !canvas || !snapshot || !snapshot->world_generated) return 0;
    draw_camera(canvas, snapshot, DISPLAY_GEOGRAPHY, 100, 0, 0, 1);
    draw_camera(canvas, snapshot, DISPLAY_CLIMATE, 100, 0, 0, 1);
    take_stamp(&before);
    sweep_draw_ok = run_zoom_sweep(canvas, snapshot, &zoom_steps);
    take_stamp(&after_zoom);
    sweep_cache_ok = immutable_equal(&before, &after_zoom);
    warm_draw_ok = run_pan_and_mode_cycles(canvas, snapshot, 100);
    GdiFlush();
    warm_draw_ok &= run_pan_and_mode_cycles(canvas, snapshot, 100);
    GdiFlush();
    take_stamp(&before_pan);
    warm_cache_ok = immutable_equal(&after_zoom, &before_pan);
    before_resources = settle_process_resources(
        &resource_settled, &resource_samples);
    cycle_draw_ok = run_pan_and_mode_cycles(canvas, snapshot, 100);
    GdiFlush();
    after_resources = process_resources();
    take_stamp(&after_pan);
    cycle_cache_ok = immutable_equal(&before_pan, &after_pan);
    resource_ok = resource_settled && before_resources.valid &&
        after_resources.valid &&
        before_resources.gdi_objects == after_resources.gdi_objects &&
        after_resources.private_bytes <= before_resources.private_bytes &&
        after_resources.working_set <= before_resources.working_set;
    fprintf(summary,
            "case=static_camera_continuous_zoom ok=%d steps=%d range=25_700_25 immutable=%d base_rebuild_delta=%d coast_draw_delta=%d river_rebuild_delta=%d wind_rebuild_delta=%d wind_sprite_prebuild_delta=%d wind_sprite_raster_delta=%d water_rebuild_delta=%d coverage_rebuild_delta=%llu tile_scan_delta=%llu coverage_source_scan_delta=%llu coverage_raster_sample_delta=%llu path_scan_delta=%llu sample_scan_delta=%llu anchor_visit_delta=%llu sprite_blit_delta=%d allocation_delta=%llu clear_pixel_delta=%llu fallback_delta=%d water_fallback_delta=%d/%d/%llu viewport_rebuild_delta=%d composition_delta=%d\n",
            sweep_draw_ok && sweep_cache_ok, zoom_steps, sweep_cache_ok,
            after_zoom.physical.base_rebuilds[0] - before.physical.base_rebuilds[0],
            after_zoom.coast.draws - before.coast.draws,
            after_zoom.overlay.river_rebuilds - before.overlay.river_rebuilds,
            after_zoom.overlay.wind_rebuilds - before.overlay.wind_rebuilds,
            after_zoom.wind.sprite_atlas_build_count -
                before.wind.sprite_atlas_build_count,
            after_zoom.wind.sprite_raster_count -
                before.wind.sprite_raster_count,
            after_zoom.water.rebuilds - before.water.rebuilds,
            (unsigned long long)(after_zoom.coverage.rebuilds -
                                 before.coverage.rebuilds),
            (unsigned long long)(after_zoom.physical.base_tile_scans[0] -
                                 before.physical.base_tile_scans[0]),
            (unsigned long long)(after_zoom.coverage.source_scans -
                                 before.coverage.source_scans),
            (unsigned long long)(after_zoom.coverage.raster_samples -
                                 before.coverage.raster_samples),
            (unsigned long long)(after_zoom.overlay.river_path_visits -
                                 before.overlay.river_path_visits),
            (unsigned long long)(after_zoom.overlay.wind_sample_visits -
                                 before.overlay.wind_sample_visits),
            (unsigned long long)(after_zoom.overlay.wind_anchor_visits -
                                 before.overlay.wind_anchor_visits),
            after_zoom.overlay.wind_sprite_blits -
                before.overlay.wind_sprite_blits,
            (unsigned long long)(after_zoom.allocation.attempts[RENDER_ALLOCATION_STATIC_MAP] -
                                 before.allocation.attempts[RENDER_ALLOCATION_STATIC_MAP]),
            (unsigned long long)(after_zoom.overlay.surface_clear_pixels -
                                 before.overlay.surface_clear_pixels),
            after_zoom.fallback_draws - before.fallback_draws,
            after_zoom.water.fallback_lake_draws -
                before.water.fallback_lake_draws,
            after_zoom.water.fallback_ocean_draws -
                before.water.fallback_ocean_draws,
            (unsigned long long)(after_zoom.water.fallback_tile_scans -
                                 before.water.fallback_tile_scans),
            after_zoom.viewport_rebuilds - before.viewport_rebuilds,
            after_zoom.static_compositions - before.static_compositions);
    fprintf(summary,
            "case=static_camera_pan_mode_100_cycles ok=%d warm=%d/%d draw=%d immutable=%d memory=%d resource_settle=%d/%d gdi=%lu->%lu private=%llu->%llu working=%llu->%llu river_mask=0x%x wind_mask=0x%x physical_bytes=%llu overlay_bytes=%llu wind_anchor_bytes=%llu wind_sprite_bytes=%llu map_bytes=%llu water_bytes=%llu water_coverage_bytes=%llu ocean_total_bytes=%llu retained_bytes=%llu anchor_visit_delta=%llu sprite_blit_delta=%d composition_delta=%d\n",
            warm_draw_ok && warm_cache_ok && cycle_draw_ok && cycle_cache_ok &&
                resource_ok, warm_draw_ok, warm_cache_ok, cycle_draw_ok,
            cycle_cache_ok, resource_ok, resource_settled, resource_samples,
            (unsigned long)before_resources.gdi_objects,
            (unsigned long)after_resources.gdi_objects,
            (unsigned long long)before_resources.private_bytes,
            (unsigned long long)after_resources.private_bytes,
            (unsigned long long)before_resources.working_set,
            (unsigned long long)after_resources.working_set,
            after_pan.overlay.ready_river_mask,
            after_pan.overlay.ready_wind_mask,
            (unsigned long long)after_pan.physical.persistent_bitmap_bytes,
            (unsigned long long)after_pan.overlay.persistent_bitmap_bytes,
            (unsigned long long)after_pan.overlay.wind_anchor_bytes,
            (unsigned long long)after_pan.overlay.wind_sprite_bytes,
            (unsigned long long)after_pan.map_memory.bitmap_bytes,
            (unsigned long long)after_pan.water_memory.bitmap_bytes,
            (unsigned long long)after_pan.coverage.retained_bytes,
            (unsigned long long)after_pan.ocean_memory.bitmap_bytes,
            (unsigned long long)(after_pan.physical.persistent_bitmap_bytes +
                                 after_pan.overlay.persistent_bitmap_bytes +
                                 after_pan.overlay.wind_anchor_bytes +
                                 after_pan.overlay.wind_sprite_bytes +
                                 after_pan.map_memory.bitmap_bytes +
                                 after_pan.scene_memory.bitmap_bytes +
                                 after_pan.ocean_memory.bitmap_bytes +
                                 after_pan.coverage.retained_bytes),
            (unsigned long long)(after_pan.overlay.wind_anchor_visits -
                                 before_pan.overlay.wind_anchor_visits),
            after_pan.overlay.wind_sprite_blits -
                before_pan.overlay.wind_sprite_blits,
            after_pan.static_compositions - before_pan.static_compositions);
    stale_ok = stale_revision_contract(summary, canvas, snapshot);
    ok = sweep_draw_ok && sweep_cache_ok && warm_draw_ok && warm_cache_ok &&
         cycle_draw_ok && cycle_cache_ok && resource_ok && stale_ok;
    display_mode = old_mode;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    map_view_auto_centered = old_centered;
    map_interaction_preview = old_preview;
    return ok;
}
