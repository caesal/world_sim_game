#include "game/game_presentation_ocean_cache_probe.h"

#include "core/game_types.h"
#include "render/render_context.h"
#include "render/render_ocean_assets.h"
#include "render/render_ocean_decoration.h"
#include "render/render_ocean_decoration_cache.h"
#include "render/render_ocean_decoration_water.h"
#include "render/render_ocean_static_mask.h"
#include "render/render_ocean_texture.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "render/render_water_coverage.h"
#include "render/render_water_surface_cache.h"
#include "ui/ui_layout.h"

#include <stdlib.h>
#include <string.h>

enum { OCEAN_CACHE_CYCLES = 100 };

typedef struct {
    int auto_run;
    int display_mode;
    int map_zoom_percent;
    int map_offset_x;
    int map_offset_y;
    int map_view_auto_centered;
    int map_interaction_preview;
    int selected_civ;
    int side_panel_collapsed;
    int ui_language;
    int world_generated;
} OceanCacheUiState;

typedef struct {
    OceanTextureDebugStats texture;
    OceanAssetDebugStats assets;
    OceanDecorationDebugStats decoration;
    OceanDecorationWaterDebugStats decoration_water;
    OceanDecorationCacheStats composites;
    RenderOceanStaticMaskStats mask;
    RenderWaterCoverageStats coverage;
    RenderWaterSurfaceCacheStats water;
    RenderLayerCacheMemory base_memory;
    RenderLayerCacheMemory ocean_memory;
    RenderLayerCacheMemory static_memory;
    OceanDecorationProbeInfo decoration_info;
    int static_compositions;
} OceanCacheStamp;

static OceanCacheUiState save_ui_state(void) {
    OceanCacheUiState state;
    state.auto_run = auto_run;
    state.display_mode = display_mode;
    state.map_zoom_percent = map_zoom_percent;
    state.map_offset_x = map_offset_x;
    state.map_offset_y = map_offset_y;
    state.map_view_auto_centered = map_view_auto_centered;
    state.map_interaction_preview = map_interaction_preview;
    state.selected_civ = selected_civ;
    state.side_panel_collapsed = side_panel_collapsed;
    state.ui_language = ui_language;
    state.world_generated = world_generated;
    return state;
}

static void restore_ui_state(OceanCacheUiState state) {
    auto_run = state.auto_run;
    display_mode = state.display_mode;
    map_zoom_percent = state.map_zoom_percent;
    map_offset_x = state.map_offset_x;
    map_offset_y = state.map_offset_y;
    map_view_auto_centered = state.map_view_auto_centered;
    map_interaction_preview = state.map_interaction_preview;
    selected_civ = state.selected_civ;
    side_panel_collapsed = state.side_panel_collapsed;
    ui_language = state.ui_language;
    world_generated = state.world_generated;
}

static void take_stamp(OceanCacheStamp *stamp) {
    memset(stamp, 0, sizeof(*stamp));
    stamp->texture = render_ocean_texture_debug_stats();
    stamp->assets = ocean_assets_debug_stats();
    stamp->decoration = ocean_decoration_debug_stats;
    stamp->decoration_water = ocean_decoration_water_debug_stats;
    stamp->composites = *ocean_decoration_cache_stats();
    stamp->mask = *render_ocean_static_mask_stats();
    stamp->coverage = *render_water_coverage_stats();
    stamp->water = *render_water_surface_cache_stats();
    stamp->base_memory = render_ocean_texture_memory();
    stamp->ocean_memory = render_ocean_decoration_memory();
    stamp->static_memory = render_static_map_cache_memory();
    stamp->decoration_info = render_ocean_decoration_probe_info();
    stamp->static_compositions = render_static_map_cache_compositions();
}

static int memory_equal(RenderLayerCacheMemory left,
                        RenderLayerCacheMemory right) {
    return left.bitmaps == right.bitmaps && left.dcs == right.dcs &&
           left.bitmap_bytes == right.bitmap_bytes;
}

static int draw_scene(StaticPhysicalProbeCanvas *canvas,
                      const RenderSnapshot *snapshot, int mode, int zoom,
                      int offset_x, int offset_y) {
    const RenderSnapshot *previous = render_context_snapshot();
    RECT client = {0, 0, canvas->width, canvas->height};
    MapLayout layout;
    display_mode = mode;
    map_zoom_percent = zoom;
    map_offset_x = offset_x;
    map_offset_y = offset_y;
    map_view_auto_centered = offset_x == 0 && offset_y == 0;
    layout = get_map_layout(client);
    static_physical_probe_canvas_clear(canvas);
    render_context_begin(snapshot);
    render_static_scene_draw(canvas->dc, client, layout, snapshot);
    render_context_end();
    if (previous) render_context_begin(previous);
    return render_static_scene_presentable();
}

static int collect_alive_ids(const RenderSnapshot *snapshot, int ids[8]) {
    int count = 0;
    int i;
    for (i = 0; i < snapshot->civ_count && i < MAX_CIVS && count < 8; i++) {
        if (snapshot->civs[i].alive) ids[count++] = i;
    }
    if (count == 0) ids[count++] = -1;
    return count;
}

static int run_cycles(StaticPhysicalProbeCanvas *canvas,
                      RenderSnapshot *snapshot, int count,
                      const int *civ_ids, int civ_id_count) {
    static const int modes[] = {
        DISPLAY_POLITICAL, DISPLAY_GEOGRAPHY, DISPLAY_CLIMATE
    };
    static const int zooms[] = {25, 100, 300, 700};
    int original_year = snapshot->year;
    int original_month = snapshot->month;
    int ok = 1;
    int i;
    for (i = 0; i < count; i++) {
        int offset_x = ((i * 37) % 241) - 120;
        int offset_y = ((i * 29) % 181) - 90;
        snapshot->year = original_year + i / 12;
        snapshot->month = i % 12 + 1;
        selected_civ = civ_ids[i % civ_id_count];
        ui_language = i & 1;
        map_interaction_preview = (i % 5) == 0;
        ok &= draw_scene(canvas, snapshot,
                         modes[i % (int)(sizeof(modes) / sizeof(modes[0]))],
                         zooms[i % (int)(sizeof(zooms) / sizeof(zooms[0]))],
                         offset_x, offset_y);
    }
    snapshot->year = original_year;
    snapshot->month = original_month;
    return ok;
}

static void reset_measured_counters(void) {
    render_ocean_texture_reset_debug();
    ocean_assets_reset_debug();
    ocean_decoration_water_reset_debug();
    render_ocean_static_mask_reset_debug();
    render_water_coverage_reset_debug();
    render_water_surface_cache_reset_debug();
}

static int base_unchanged(const OceanCacheStamp *before,
                          const OceanCacheStamp *after) {
    return after->texture.rebuilds == before->texture.rebuilds &&
        after->texture.allocation_rebuilds ==
            before->texture.allocation_rebuilds &&
        after->texture.generation == before->texture.generation &&
        after->texture.identity == before->texture.identity &&
        after->texture.source_identity == before->texture.source_identity &&
        after->texture.bitmap_identity == before->texture.bitmap_identity &&
        after->texture.dc_identity == before->texture.dc_identity &&
        after->texture.tile_px == 760 && after->texture.phase_x == 0 &&
        after->texture.phase_y == 0 && after->texture.resample_calls == 0 &&
        after->texture.stretchblt_calls == 0 &&
        after->assets.texture_decode_attempts ==
            before->assets.texture_decode_attempts &&
        after->assets.texture_raster_calls ==
            before->assets.texture_raster_calls &&
        after->assets.texture_tile_draw_calls ==
            before->assets.texture_tile_draw_calls &&
        memory_equal(before->base_memory, after->base_memory);
}

static int decoration_unchanged(const OceanCacheStamp *before,
                                const OceanCacheStamp *after) {
    return after->decoration.composite_rebuilds ==
               before->decoration.composite_rebuilds &&
        after->decoration.composite_presents ==
               before->decoration.composite_presents &&
        after->decoration.exterior_layer_rebuilds ==
               before->decoration.exterior_layer_rebuilds &&
        after->decoration.interior_layer_rebuilds ==
               before->decoration.interior_layer_rebuilds &&
        after->decoration.exterior_layer_allocations ==
               before->decoration.exterior_layer_allocations &&
        after->decoration.interior_layer_allocations ==
               before->decoration.interior_layer_allocations &&
        after->decoration.exterior_layer_clears ==
               before->decoration.exterior_layer_clears &&
        after->decoration.interior_layer_clears ==
               before->decoration.interior_layer_clears &&
        after->decoration.exterior_cleared_pixels ==
               before->decoration.exterior_cleared_pixels &&
        after->decoration.interior_cleared_pixels ==
               before->decoration.interior_cleared_pixels &&
        after->decoration.exterior_surface_identity ==
               before->decoration.exterior_surface_identity &&
        after->decoration.interior_surface_identity ==
               before->decoration.interior_surface_identity &&
        after->decoration.exterior_layer_key ==
               before->decoration.exterior_layer_key &&
        after->decoration.interior_layer_key ==
               before->decoration.interior_layer_key &&
        after->assets.motif_decode_attempts ==
               before->assets.motif_decode_attempts &&
        after->assets.motif_draw_calls == before->assets.motif_draw_calls &&
        after->assets.motif_draw_failures ==
               before->assets.motif_draw_failures &&
        after->decoration_info.item_rebuilds ==
               before->decoration_info.item_rebuilds &&
        after->composites.exterior_retained_bytes ==
               before->composites.exterior_retained_bytes &&
        after->composites.interior_retained_bytes ==
               before->composites.interior_retained_bytes;
}

int game_presentation_ocean_cache_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot) {
    OceanCacheUiState old_state = save_ui_state();
    StaticPhysicalProbeCanvas resized = {0};
    RenderSnapshot *cycle_snapshot = NULL;
    OceanCacheStamp before = {0}, after = {0};
    OceanCacheStamp resize_before = {0}, resize_first = {0};
    OceanCacheStamp resize_second = {0};
    int civ_ids[8];
    int civ_id_count = 0;
    DWORD gdi_before = 0, gdi_after = 0;
    DWORD resize_gdi_first = 0, resize_gdi_second = 0;
    int warm_ok = 0, cycles_ok = 0, immutable_ok = 0;
    int scan_ok = 0, memory_ok = 0, resource_ok = 0;
    int resize_open = 0, resize_draw_first = 0, resize_draw_second = 0;
    int resize_once = 0, resize_stable = 0, ok = 0;
    if (!summary || !canvas || !canvas->pixels || !snapshot ||
        !snapshot->world_generated) return 0;
    cycle_snapshot = (RenderSnapshot *)malloc(sizeof(*cycle_snapshot));
    if (!cycle_snapshot) goto cleanup;
    memcpy(cycle_snapshot, snapshot, sizeof(*cycle_snapshot));
    civ_id_count = collect_alive_ids(snapshot, civ_ids);
    auto_run = 0;
    side_panel_collapsed = 1;
    ui_language = UI_LANG_EN;
    map_interaction_preview = 0;
    world_generated = 1;

    warm_ok = draw_scene(canvas, cycle_snapshot, DISPLAY_POLITICAL,
                         100, 0, 0);
    warm_ok &= draw_scene(canvas, cycle_snapshot, DISPLAY_GEOGRAPHY,
                          100, 0, 0);
    warm_ok &= draw_scene(canvas, cycle_snapshot, DISPLAY_CLIMATE,
                          100, 0, 0);
    warm_ok &= run_cycles(canvas, cycle_snapshot, OCEAN_CACHE_CYCLES,
                          civ_ids, civ_id_count);
    GdiFlush();
    reset_measured_counters();
    take_stamp(&before);
    gdi_before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    cycles_ok = run_cycles(canvas, cycle_snapshot, OCEAN_CACHE_CYCLES,
                           civ_ids, civ_id_count);
    GdiFlush();
    gdi_after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    take_stamp(&after);

    immutable_ok = base_unchanged(&before, &after) &&
                   decoration_unchanged(&before, &after);
    scan_ok = after.coverage.rebuilds == before.coverage.rebuilds &&
        after.coverage.source_scans == before.coverage.source_scans &&
        after.coverage.raster_samples == before.coverage.raster_samples &&
        after.mask.applications == before.mask.applications &&
        after.mask.pixel_scans == before.mask.pixel_scans &&
        after.mask.coverage_samples == before.mask.coverage_samples &&
        after.water.rebuilds == before.water.rebuilds &&
        after.water.mask_pixel_scans == before.water.mask_pixel_scans &&
        after.decoration_water.motif_clearance_calls ==
            before.decoration_water.motif_clearance_calls &&
        after.decoration_water.motif_clearance_tile_visits ==
            before.decoration_water.motif_clearance_tile_visits &&
        after.static_compositions == before.static_compositions;
    memory_ok = memory_equal(before.base_memory, after.base_memory) &&
        memory_equal(before.ocean_memory, after.ocean_memory) &&
        memory_equal(before.static_memory, after.static_memory) &&
        before.coverage.retained_bytes == after.coverage.retained_bytes;
    resource_ok = gdi_before > 0 && gdi_before == gdi_after;

    resize_open = static_physical_probe_canvas_open(
        &resized, canvas->width + 64, canvas->height + 48);
    if (resize_open) {
        take_stamp(&resize_before);
        resize_draw_first = draw_scene(&resized, cycle_snapshot,
                                       DISPLAY_GEOGRAPHY, 100, 0, 0);
        GdiFlush();
        resize_gdi_first = GetGuiResources(GetCurrentProcess(),
                                           GR_GDIOBJECTS);
        take_stamp(&resize_first);
        resize_draw_second = draw_scene(&resized, cycle_snapshot,
                                        DISPLAY_GEOGRAPHY, 100, 0, 0);
        GdiFlush();
        resize_gdi_second = GetGuiResources(GetCurrentProcess(),
                                            GR_GDIOBJECTS);
        take_stamp(&resize_second);
    }
    resize_once = resize_open && resize_draw_first &&
        resize_first.texture.rebuilds == resize_before.texture.rebuilds + 1 &&
        resize_first.texture.allocation_rebuilds ==
            resize_before.texture.allocation_rebuilds + 1 &&
        resize_first.texture.generation == resize_before.texture.generation + 1 &&
        resize_first.assets.texture_decode_attempts ==
            resize_before.assets.texture_decode_attempts &&
        resize_first.assets.texture_raster_calls ==
            resize_before.assets.texture_raster_calls + 1 &&
        resize_first.texture.width == resized.width &&
        resize_first.texture.height == resized.height &&
        resize_first.texture.persistent_bytes ==
            (uint64_t)resized.width * (uint64_t)resized.height * 4u &&
        resize_first.texture.tile_px == 760 &&
        resize_first.texture.phase_x == 0 &&
        resize_first.texture.phase_y == 0 &&
        resize_first.texture.resample_calls == 0 &&
        resize_first.texture.stretchblt_calls == 0 &&
        resize_first.decoration_water.motif_clearance_calls ==
            resize_before.decoration_water.motif_clearance_calls &&
        resize_first.decoration_water.motif_clearance_tile_visits ==
            resize_before.decoration_water.motif_clearance_tile_visits &&
        resize_first.decoration.composite_rebuilds ==
            resize_before.decoration.composite_rebuilds &&
        resize_first.decoration.exterior_layer_rebuilds ==
            resize_before.decoration.exterior_layer_rebuilds + 1 &&
        resize_first.decoration.interior_layer_rebuilds ==
            resize_before.decoration.interior_layer_rebuilds &&
        resize_first.decoration.exterior_layer_allocations ==
            resize_before.decoration.exterior_layer_allocations + 1 &&
        resize_first.decoration.interior_layer_allocations ==
            resize_before.decoration.interior_layer_allocations &&
        resize_first.decoration.exterior_layer_clears ==
            resize_before.decoration.exterior_layer_clears + 1 &&
        resize_first.decoration.interior_layer_clears ==
            resize_before.decoration.interior_layer_clears &&
        resize_first.assets.motif_draw_calls ==
            resize_before.assets.motif_draw_calls +
                (uint64_t)resize_before.decoration_info.exterior_items &&
        resize_first.decoration.interior_surface_identity ==
            resize_before.decoration.interior_surface_identity;
    resize_stable = resize_draw_second &&
        base_unchanged(&resize_first, &resize_second) &&
        decoration_unchanged(&resize_first, &resize_second) &&
        resize_second.coverage.rebuilds == resize_first.coverage.rebuilds &&
        resize_second.coverage.source_scans ==
            resize_first.coverage.source_scans &&
        resize_second.mask.applications == resize_first.mask.applications &&
        resize_second.mask.pixel_scans == resize_first.mask.pixel_scans &&
        resize_second.decoration_water.motif_clearance_calls ==
            resize_first.decoration_water.motif_clearance_calls &&
        resize_second.decoration_water.motif_clearance_tile_visits ==
            resize_first.decoration_water.motif_clearance_tile_visits &&
        memory_equal(resize_first.ocean_memory, resize_second.ocean_memory) &&
        memory_equal(resize_first.static_memory, resize_second.static_memory) &&
        resize_gdi_first > 0 && resize_gdi_first == resize_gdi_second;
    ok = warm_ok && cycles_ok && immutable_ok && scan_ok && memory_ok &&
         resource_ok && resize_once && resize_stable;

    fprintf(summary,
            "case=ocean_cache_100_cycles ok=%d warm=%d cycles=%d "
            "civs=%d base_rebuild_delta=%llu allocation_delta=%llu "
            "decode_delta=%llu raster_delta=%llu coverage_delta=%llu/%llu "
            "mask_delta=%llu/%llu water_delta=%d/%llu "
            "motif_clearance_delta=%llu/%llu "
            "static_composition_delta=%d composite_rebuild_delta=%llu "
            "motif_draw_delta=%llu exterior_rebuild_delta=%llu "
            "interior_rebuild_delta=%llu layer_alloc_delta=%llu/%llu "
            "layer_clear_delta=%llu/%llu "
            "base_bytes=%llu ocean_bytes=%llu static_bytes=%llu "
            "coverage_bytes=%llu gdi=%lu/%lu\n",
            warm_ok && cycles_ok && immutable_ok && scan_ok && memory_ok &&
                resource_ok,
            warm_ok, cycles_ok, civ_id_count,
            (unsigned long long)(after.texture.rebuilds -
                                 before.texture.rebuilds),
            (unsigned long long)(after.texture.allocation_rebuilds -
                                 before.texture.allocation_rebuilds),
            (unsigned long long)(after.assets.texture_decode_attempts -
                                 before.assets.texture_decode_attempts),
            (unsigned long long)(after.assets.texture_raster_calls -
                                 before.assets.texture_raster_calls),
            (unsigned long long)(after.coverage.rebuilds -
                                 before.coverage.rebuilds),
            (unsigned long long)(after.coverage.source_scans -
                                 before.coverage.source_scans),
            (unsigned long long)(after.mask.applications -
                                 before.mask.applications),
            (unsigned long long)(after.mask.pixel_scans -
                                 before.mask.pixel_scans),
            after.water.rebuilds - before.water.rebuilds,
            (unsigned long long)(after.water.mask_pixel_scans -
                                 before.water.mask_pixel_scans),
            (unsigned long long)(
                after.decoration_water.motif_clearance_calls -
                before.decoration_water.motif_clearance_calls),
            (unsigned long long)(
                after.decoration_water.motif_clearance_tile_visits -
                before.decoration_water.motif_clearance_tile_visits),
            after.static_compositions - before.static_compositions,
            (unsigned long long)(after.decoration.composite_rebuilds -
                                 before.decoration.composite_rebuilds),
            (unsigned long long)(after.assets.motif_draw_calls -
                                 before.assets.motif_draw_calls),
            (unsigned long long)(after.decoration.exterior_layer_rebuilds -
                                 before.decoration.exterior_layer_rebuilds),
            (unsigned long long)(after.decoration.interior_layer_rebuilds -
                                 before.decoration.interior_layer_rebuilds),
            (unsigned long long)(after.decoration.exterior_layer_allocations -
                                 before.decoration.exterior_layer_allocations),
            (unsigned long long)(after.decoration.interior_layer_allocations -
                                 before.decoration.interior_layer_allocations),
            (unsigned long long)(after.decoration.exterior_layer_clears -
                                 before.decoration.exterior_layer_clears),
            (unsigned long long)(after.decoration.interior_layer_clears -
                                 before.decoration.interior_layer_clears),
            (unsigned long long)after.base_memory.bitmap_bytes,
            (unsigned long long)after.ocean_memory.bitmap_bytes,
            (unsigned long long)after.static_memory.bitmap_bytes,
            (unsigned long long)after.coverage.retained_bytes,
            (unsigned long)gdi_before, (unsigned long)gdi_after);
    fprintf(summary,
            "case=ocean_cache_resize_once ok=%d first=%d repeat=%d "
            "geometry=%dx%d rebuild_delta=%llu allocation_delta=%llu "
            "generation_delta=%llu decode_delta=%llu raster_delta=%llu "
            "repeat_rebuild_delta=%llu repeat_raster_delta=%llu "
            "motif_clearance_delta=%llu/%llu bytes=%llu "
            "repeat_memory=%d gdi=%lu/%lu\n",
            resize_once && resize_stable, resize_draw_first,
            resize_draw_second, resized.width, resized.height,
            (unsigned long long)(resize_first.texture.rebuilds -
                                 resize_before.texture.rebuilds),
            (unsigned long long)(resize_first.texture.allocation_rebuilds -
                                 resize_before.texture.allocation_rebuilds),
            (unsigned long long)(resize_first.texture.generation -
                                 resize_before.texture.generation),
            (unsigned long long)(resize_first.assets.texture_decode_attempts -
                                 resize_before.assets.texture_decode_attempts),
            (unsigned long long)(resize_first.assets.texture_raster_calls -
                                 resize_before.assets.texture_raster_calls),
            (unsigned long long)(resize_second.texture.rebuilds -
                                 resize_first.texture.rebuilds),
            (unsigned long long)(resize_second.assets.texture_raster_calls -
                                 resize_first.assets.texture_raster_calls),
            (unsigned long long)(
                resize_second.decoration_water.motif_clearance_calls -
                resize_before.decoration_water.motif_clearance_calls),
            (unsigned long long)(
                resize_second.decoration_water.motif_clearance_tile_visits -
                resize_before.decoration_water.motif_clearance_tile_visits),
            (unsigned long long)resize_second.base_memory.bitmap_bytes,
            memory_equal(resize_first.ocean_memory,
                         resize_second.ocean_memory),
            (unsigned long)resize_gdi_first,
            (unsigned long)resize_gdi_second);

cleanup:
    static_physical_probe_canvas_close(&resized);
    free(cycle_snapshot);
    restore_ui_state(old_state);
    return ok;
}
