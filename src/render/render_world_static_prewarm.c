#include "render/render_world_static_prewarm.h"

#include "core/worldgen_attempt.h"
#include "render/map_label_cache.h"
#include "render/panel_map_water_legend.h"
#include "render/render_ocean_decoration.h"
#include "render/render_context.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_static_scene.h"
#include "render/render_water_surface_cache.h"
#include "ui/ui_types.h"

static RenderWorldStaticPrewarmStats stats;

static int prewarm_mode(HDC hdc, RECT client, MapLayout layout,
                        const RenderSnapshot *snapshot, int mode) {
    int attempt;
    display_mode = mode;
    for (attempt = 0; attempt < 2; attempt++) {
        render_static_scene_draw(hdc, client, layout, snapshot);
        if (render_static_scene_fully_current()) break;
    }
    if (!render_static_scene_fully_current()) return 0;
    map_label_cache_draw_labels(hdc, client, layout, snapshot);
    stats.modes_completed++;
    return 1;
}

int render_world_static_prewarm_layout(HDC hdc, RECT client, MapLayout layout,
                                       int zoom_percent,
                                       const RenderSnapshot *snapshot) {
    DWORD start = GetTickCount();
    int old_mode = display_mode;
    int old_zoom = map_zoom_percent;
    int old_preview = map_interaction_preview;
    int ok;
    stats.attempts++;
    if (!hdc || !snapshot || !snapshot->world_generated) {
        stats.failures++;
        return 0;
    }
    map_interaction_preview = 0;
    map_zoom_percent = clamp(zoom_percent, 25, 700);
    ok = render_water_surface_cache_ensure(hdc, snapshot) &&
         render_water_surface_cache_ready(snapshot);
    if (ok) ok = panel_map_water_legend_prewarm(hdc);
    if (ok) ok = render_ocean_decoration_prewarm_background(
        hdc, client, layout, snapshot);
    if (ok)
        ok = render_static_physical_overlay_cache_prewarm_all(hdc, snapshot);
    render_context_begin(snapshot);
    if (ok) ok = prewarm_mode(hdc, client, layout, snapshot, DISPLAY_GEOGRAPHY);
    if (ok) ok = prewarm_mode(hdc, client, layout, snapshot, DISPLAY_CLIMATE);
    render_context_end();
    display_mode = old_mode;
    map_zoom_percent = old_zoom;
    map_interaction_preview = old_preview;
    stats.last_ms = (int)(GetTickCount() - start);
    if (ok) {
        stats.completions++;
        stats.layouts_completed++;
    } else stats.failures++;
    return ok;
}

int render_world_static_prewarm_from_published(HWND hwnd) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    RECT client = {0, 0, 1280, 800};
    MapLayout current_layout;
    HDC hdc;
    HWND dc_owner = hwnd;
    int current_zoom = map_zoom_percent;
    int ok = 0;
    stats.requests++;
    if (!snapshot || !snapshot->world_generated) {
        if (snapshot) render_snapshot_release(snapshot);
        stats.failures++;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREWARM_BUILD);
        return 0;
    }
    if (hwnd && !GetClientRect(hwnd, &client)) {
        render_snapshot_release(snapshot);
        stats.failures++;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREWARM_BUILD);
        return 0;
    }
    current_layout = get_map_layout(client);
    hdc = GetDC(hwnd);
    if (!hdc && hwnd) {
        dc_owner = NULL;
        hdc = GetDC(NULL);
    }
    if (hdc) {
        ok = render_static_physical_cache_prewarm(hdc, snapshot);
        if (!ok) stats.failures++;
        if (ok) ok = render_world_static_prewarm_layout(
            hdc, client, current_layout, current_zoom, snapshot);
        ReleaseDC(dc_owner, hdc);
    } else {
        stats.failures++;
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREWARM_BUILD);
    }
    render_snapshot_release(snapshot);
    if (!ok) worldgen_attempt_record_failure(WORLDGEN_FAILURE_PREWARM_BUILD);
    return ok;
}

const RenderWorldStaticPrewarmStats *render_world_static_prewarm_stats(void) {
    return &stats;
}
