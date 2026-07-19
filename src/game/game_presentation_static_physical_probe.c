#include "game/game_presentation_static_physical_probe.h"

#include "core/constants.h"
#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "game/game_presentation_static_camera_probe.h"
#include "game/game_presentation_static_physical_artifacts.h"
#include "game/game_presentation_static_physical_metrics.h"
#include "game/game_presentation_water_river_probe.h"
#include "game/game_worldgen.h"
#include "render/coast_geometry.h"
#include "render/render_context.h"
#include "render/render_ocean_decoration.h"
#include "render/render_ocean_decoration_cache.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_static_scene.h"
#include "render/render_world_static_prewarm.h"
#include "render/river_geometry.h"
#include "render/river_render.h"
#include "render/wind_render.h"
#include "ui/ui_layout.h"
#include "world/world_gen.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum {
    PROBE_W = 1152,
    PROBE_H = 800,
    FULL_ZOOM = 100,
    CLOSE_ZOOM = 700
};

typedef struct {
    int auto_run;
    int map_interaction_preview;
    int map_view_auto_centered;
    int display_mode;
    int map_zoom_percent;
    int map_offset_x;
    int map_offset_y;
    int side_panel_collapsed;
} ProbeUiState;

static unsigned int presentation_world_seed(void) {
    const char *text = getenv("WORLD_SIM_PRESENTATION_WORLD_SEED");
    char *end = NULL;
    unsigned long long value;
    if (!text || !text[0]) return 731905u;
    value = strtoull(text, &end, 10);
    if (!end || end == text || *end != '\0' || value > UINT_MAX)
        return 731905u;
    return (unsigned int)value;
}

static ProbeUiState save_ui_state(void) {
    ProbeUiState state;
    state.auto_run = auto_run;
    state.map_interaction_preview = map_interaction_preview;
    state.map_view_auto_centered = map_view_auto_centered;
    state.display_mode = display_mode;
    state.map_zoom_percent = map_zoom_percent;
    state.map_offset_x = map_offset_x;
    state.map_offset_y = map_offset_y;
    state.side_panel_collapsed = side_panel_collapsed;
    return state;
}

static void restore_ui_state(ProbeUiState state) {
    auto_run = state.auto_run;
    map_interaction_preview = state.map_interaction_preview;
    map_view_auto_centered = state.map_view_auto_centered;
    display_mode = state.display_mode;
    map_zoom_percent = state.map_zoom_percent;
    map_offset_x = state.map_offset_x;
    map_offset_y = state.map_offset_y;
    side_panel_collapsed = state.side_panel_collapsed;
}

static MapLayout layout_for(RECT client, int mode, int zoom) {
    display_mode = mode;
    map_zoom_percent = zoom;
    map_offset_x = 0;
    map_offset_y = 0;
    map_view_auto_centered = 1;
    map_interaction_preview = 0;
    return get_map_layout(client);
}

static int draw_variant(StaticPhysicalProbeCanvas *canvas,
                        const RenderSnapshot *snapshot, int mode, int zoom,
                        const char *artifact,
                        StaticPhysicalWindVisualMetrics *visual) {
    RECT client = {0, 0, PROBE_W, PROBE_H};
    MapLayout layout = layout_for(client, mode, zoom);
    auto_run = 0;
    static_physical_probe_canvas_clear(canvas);
    render_context_begin(snapshot);
    render_static_scene_draw(canvas->dc, client, layout, snapshot);
    render_static_scene_draw(canvas->dc, client, layout, snapshot);
    render_context_end();
    GdiFlush();
    if (visual)
        static_physical_probe_analyze_wind(canvas, snapshot, client,
                                           layout, visual);
    return (!artifact || static_physical_probe_canvas_write(
                                canvas, static_physical_probe_artifact_dir(),
                                artifact)) &&
           render_static_scene_presentable();
}

static int generate_real_snapshot(const RenderSnapshot **out,
                                  unsigned int seed) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    WorldGenContext *context;
    int committed;
    config.seed = seed;
    config.random_seed = 0;
    config.ocean = 47;
    config.continent = 58;
    config.relief = 61;
    config.moisture = 43;
    config.drought = 52;
    config.vegetation = 57;
    config.bias_forest = 62;
    config.bias_desert = 41;
    config.bias_mountain = 66;
    config.bias_wetland = 39;
    game_clear_world_tiles();
    set_active_map_size(MAP_SIZE_EXTREME);
    world_generated = 0;
    context = world_gen_prepare_for_dimensions(&config, MAX_MAP_W, MAX_MAP_H);
    if (!context || !world_gen_prepared_can_commit(context, MAX_MAP_W, MAX_MAP_H)) {
        world_gen_release_prepared(context);
        return 0;
    }
    committed = world_gen_commit_prepared(context);
    world_gen_release_prepared(context);
    if (!committed) return 0;
    world_generated = 1;
    dirty_mark_world();
    render_snapshot_publish_from_live_state();
    *out = render_snapshot_acquire();
    return *out && (*out)->world_generated && (*out)->map_w == MAX_MAP_W &&
           (*out)->map_h == MAX_MAP_H && (*out)->wind.valid &&
           (*out)->rivers.valid;
}

static void river_lod_counts(int *total, int *full, int *close) {
    const RiverRenderPath *paths = river_geometry_paths(total);
    int close_lod = river_render_lod_bucket_for_zoom(CLOSE_ZOOM, 6);
    int i;
    *full = 0;
    *close = 0;
    for (i = 0; paths && i < *total; i++) {
        *full += river_render_path_visible_at_lod(&paths[i], 0);
        *close += river_render_path_visible_at_lod(&paths[i], close_lod);
    }
}

static void reset_attempt_caches(void) {
    render_static_map_cache_invalidate_all();
    render_static_scene_invalidate_cache();
    ocean_decoration_cache_invalidate();
    render_static_scene_reset_debug();
    render_static_physical_cache_reset_debug_counters();
    coast_geometry_reset_debug();
    render_static_physical_overlay_cache_reset_debug_counters();
    render_ocean_decoration_reset_debug();
    ocean_decoration_cache_reset_debug();
    wind_render_reset_debug_counters();
}

static int prewarm_attempt(StaticPhysicalProbeCanvas *canvas,
                           const RenderSnapshot *snapshot) {
    RECT client = {0, 0, PROBE_W, PROBE_H};
    MapLayout full = layout_for(client, DISPLAY_GEOGRAPHY, FULL_ZOOM);
    int ok = render_static_physical_cache_prewarm(canvas->dc, snapshot);
    ok &= render_world_static_prewarm_layout(
        canvas->dc, client, full, FULL_ZOOM, snapshot);
    return ok;
}

int game_presentation_static_physical_probe(FILE *summary) {
    const RenderSnapshot *snapshot = NULL;
    StaticPhysicalProbeCanvas canvas;
    StaticPhysicalWindVisualMetrics gf = {0}, cf = {0}, gc = {0}, cc = {0};
    StaticPhysicalWindVisualMetrics post = {0};
    StaticPhysicalWindVisualMetrics wind_zoom[5] = {{0}};
    StaticPhysicalLabelReuseMetrics labels = {0};
    RenderStaticPhysicalCacheStats prewarm_physical = {0};
    RenderStaticPhysicalOverlayCacheStats prewarm_overlay = {0};
    CoastGeometryStats prewarm_coast = {0};
    ProbeUiState old_state = save_ui_state();
    int total = 0, full = 0, close = 0;
    int generated = 0, wind_contract_ok = 0, prewarm_ok = 0;
    int artifacts_ok = 1, visuals_ok = 0, width_ok = 1;
    int lod_ok = 0, ready_ok = 0;
    int label_ok = 0, hash_ok = 0, water_river_ok = 0, camera_ok = 0;
    int ok = 0, i;
    unsigned int seed = presentation_world_seed();
    memset(&canvas, 0, sizeof(canvas));
    if (!summary || !static_physical_probe_prepare_artifact_dir() ||
        !static_physical_probe_canvas_open(&canvas, PROBE_W, PROBE_H))
        goto cleanup;
    side_panel_collapsed = 1;
    auto_run = 0;
    map_interaction_preview = 0;
    generated = generate_real_snapshot(&snapshot, seed);
    if (!generated) {
        fprintf(summary,
                "case=static_physical_generated_world ok=0 "
                "reason=generation seed=%u artifact_dir=%s\n",
                seed, static_physical_probe_artifact_dir());
        goto cleanup;
    }
    wind_contract_ok = snapshot->wind.coarse_count == SNAPSHOT_WIND_COARSE_MAX &&
                       snapshot->wind.medium_count == SNAPSHOT_WIND_MEDIUM_MAX &&
                       snapshot->wind.fine_count == SNAPSHOT_WIND_FINE_MAX;
    reset_attempt_caches();
    prewarm_ok = prewarm_attempt(&canvas, snapshot);
    prewarm_physical = *render_static_physical_cache_stats();
    prewarm_overlay = *render_static_physical_overlay_cache_stats();
    prewarm_coast = *coast_geometry_stats();
    ready_ok = prewarm_ok && prewarm_physical.ready_base_mask == 0x07u &&
               prewarm_physical.coast_ready &&
               prewarm_overlay.ready_river_mask == 0x0fu &&
               prewarm_overlay.ready_wind_mask == 0x07u;
    artifacts_ok &= draw_variant(&canvas, snapshot, DISPLAY_GEOGRAPHY,
                                 FULL_ZOOM,
                                 "static_physical_geography_full.bmp", &gf);
    artifacts_ok &= draw_variant(&canvas, snapshot, DISPLAY_CLIMATE,
                                 FULL_ZOOM,
                                 "static_physical_climate_full.bmp", &cf);
    artifacts_ok &= draw_variant(&canvas, snapshot, DISPLAY_GEOGRAPHY,
                                 CLOSE_ZOOM,
                                 "static_physical_geography_close.bmp", &gc);
    artifacts_ok &= draw_variant(&canvas, snapshot, DISPLAY_CLIMATE,
                                 CLOSE_ZOOM,
                                 "static_physical_climate_close.bmp", &cc);
    artifacts_ok &= draw_variant(&canvas, snapshot, DISPLAY_GEOGRAPHY,
                                 25, NULL, &wind_zoom[0]);
    artifacts_ok &= draw_variant(&canvas, snapshot, DISPLAY_GEOGRAPHY,
                                 50, NULL, &wind_zoom[1]);
    wind_zoom[2] = gf;
    artifacts_ok &= draw_variant(&canvas, snapshot, DISPLAY_GEOGRAPHY,
                                 200, NULL, &wind_zoom[3]);
    wind_zoom[4] = gc;
    for (i = 0; i < 5; i++) {
        width_ok &= wind_zoom[i].composite_width_samples > 0 &&
                    wind_zoom[i].composite_width_min >= 3 &&
                    wind_zoom[i].composite_width_max <= 5;
        if (i > 0)
            width_ok &= abs(wind_zoom[i].composite_width_min -
                            wind_zoom[0].composite_width_min) <= 1;
    }
    river_lod_counts(&total, &full, &close);
    lod_ok = total > 0 && full > 0 && close > full && close <= total;
    visuals_ok = static_physical_probe_wind_style_contract() && width_ok &&
                 gf.eligible > 0 && cf.eligible > 0 &&
                 gc.eligible > 0 && cc.eligible > 0 &&
                 gf.recognized * 4 >= gf.eligible &&
                 cf.recognized * 4 >= cf.eligible &&
                 gc.recognized * 2 >= gc.eligible &&
                 cc.recognized * 2 >= cc.eligible &&
                 gf.hash != cf.hash && gc.hash != cc.hash &&
                 gf.hash != gc.hash;
    camera_ok = game_presentation_static_camera_probe(summary, &canvas, snapshot);
    artifacts_ok &= draw_variant(&canvas, snapshot, DISPLAY_GEOGRAPHY,
                                 FULL_ZOOM, NULL, &post);
    hash_ok = post.hash == gf.hash;
    map_zoom_percent = FULL_ZOOM;
    map_interaction_preview = 0;
    label_ok = static_physical_probe_label_reuse(
        canvas.dc, (RECT){0, 0, PROBE_W, PROBE_H}, &labels);
    water_river_ok = game_presentation_water_river_probe(
        summary, &canvas, snapshot);
    fprintf(summary,
            "case=static_physical_generated_world ok=%d seed=%u map=%dx%d wind=%d/%d/%d rivers=%d artifact_dir=%s\n",
            generated && ready_ok && wind_contract_ok, seed, snapshot->map_w,
            snapshot->map_h, snapshot->wind.coarse_count,
            snapshot->wind.medium_count, snapshot->wind.fine_count,
            snapshot->rivers.path_count,
            static_physical_probe_artifact_dir());
    fprintf(summary,
            "case=static_physical_wind_geometry_visual ok=%d style=%d geography_full=%d/%d climate_full=%d/%d geography_close=%d/%d climate_close=%d/%d components=%d/%d/%d core_samples=%d halo_samples=%d sampled=%d\n",
            visuals_ok, static_physical_probe_wind_style_contract(),
            gf.recognized, gf.eligible, cf.recognized, cf.eligible,
            gc.recognized, gc.eligible, cc.recognized, cc.eligible,
            gf.shaft_recognized + cf.shaft_recognized +
                gc.shaft_recognized + cc.shaft_recognized,
            gf.head_left_recognized + cf.head_left_recognized +
                gc.head_left_recognized + cc.head_left_recognized,
            gf.head_right_recognized + cf.head_right_recognized +
                gc.head_right_recognized + cc.head_right_recognized,
            gf.core_sample_hits + cf.core_sample_hits +
                gc.core_sample_hits + cc.core_sample_hits,
            gf.halo_sample_hits + cf.halo_sample_hits +
                gc.halo_sample_hits + cc.halo_sample_hits,
            gf.sampled_points + cf.sampled_points +
                gc.sampled_points + cc.sampled_points);
    fprintf(summary,
            "case=static_physical_wind_stroke_scaling ok=%d "
            "zoom25=%d-%d/%d zoom50=%d-%d/%d zoom100=%d-%d/%d "
            "zoom200=%d-%d/%d zoom700=%d-%d/%d expected=3..5\n",
            width_ok,
            wind_zoom[0].composite_width_min,
            wind_zoom[0].composite_width_max,
            wind_zoom[0].composite_width_samples,
            wind_zoom[1].composite_width_min,
            wind_zoom[1].composite_width_max,
            wind_zoom[1].composite_width_samples,
            wind_zoom[2].composite_width_min,
            wind_zoom[2].composite_width_max,
            wind_zoom[2].composite_width_samples,
            wind_zoom[3].composite_width_min,
            wind_zoom[3].composite_width_max,
            wind_zoom[3].composite_width_samples,
            wind_zoom[4].composite_width_min,
            wind_zoom[4].composite_width_max,
            wind_zoom[4].composite_width_samples);
    fprintf(summary,
            "case=static_physical_river_lod ok=%d total=%d full_lod_visible=%d close_lod_visible=%d suppressed_full=%d suppressed_close=%d\n",
            lod_ok, total, full, close, total - full, total - close);
    fprintf(summary,
            "case=static_physical_prewarm ok=%d base_mask=0x%x coast=%d river_mask=0x%x wind_mask=0x%x base_bitmaps=%d overlay_bitmaps=%d bytes=%llu/%llu coast_regularized=%llu regularization_transient=%llu wind_anchor_bytes=%llu wind_sprite_atlas=%dx%d/%llu wind_sprite_prebuilds=%d wind_sprite_rasters=%d coast_segments=%d coast_bytes=%llu\n",
            ready_ok, prewarm_physical.ready_base_mask,
            prewarm_physical.coast_ready,
            prewarm_overlay.ready_river_mask,
            prewarm_overlay.ready_wind_mask,
            prewarm_physical.persistent_bitmaps,
            prewarm_overlay.persistent_bitmaps,
            (unsigned long long)prewarm_physical.persistent_bitmap_bytes,
            (unsigned long long)prewarm_overlay.persistent_bitmap_bytes,
            (unsigned long long)prewarm_physical.coast_regularized_tiles,
            (unsigned long long)prewarm_physical.coast_regularization_transient_bytes,
            (unsigned long long)prewarm_overlay.wind_anchor_bytes,
            wind_render_stats()->sprite_atlas_width,
            wind_render_stats()->sprite_atlas_height,
            (unsigned long long)prewarm_overlay.wind_sprite_bytes,
            wind_render_stats()->sprite_atlas_build_count,
            wind_render_stats()->sprite_raster_count,
            prewarm_coast.segment_count,
            (unsigned long long)prewarm_coast.retained_bytes);
    fprintf(summary,
            "case=static_physical_post_cycle_hash ok=%d geography_full=%llu after_cycles=%llu\n",
            hash_ok, (unsigned long long)gf.hash,
            (unsigned long long)post.hash);
    fprintf(summary,
            "case=static_physical_label_gcgc_reuse ok=%d fixture=%d/%d candidates=%d drawn=%d source=%d->%d placement=%d->%d pool_hit=%d store=%d\n",
            label_ok, labels.fixture_civs, labels.fixture_cities,
            labels.candidates, labels.drawn,
            labels.source_rebuilds_after_first,
            labels.source_rebuilds_after_cycle,
            labels.placement_rebuilds_after_first,
            labels.placement_rebuilds_after_cycle,
            labels.placement_pool_hits, labels.placement_pool_stores);
    ok = generated && wind_contract_ok && ready_ok && artifacts_ok &&
         visuals_ok && lod_ok && camera_ok && hash_ok && label_ok &&
         water_river_ok;

cleanup:
    if (snapshot) render_snapshot_release(snapshot);
    static_physical_probe_canvas_close(&canvas);
    restore_ui_state(old_state);
    return ok;
}
