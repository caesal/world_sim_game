#include "render/render_static_scene.h"

#include "core/profiler.h"
#include "render/map_display_policy.h"
#include "render/profiling_switches.h"
#include "render/render_ocean_decoration.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_water_surface_cache.h"
#include "render/river_render.h"
#include "render/wind_render.h"
#include "sim/simulation_worker.h"
#include "ui/ui_invalidation.h"

#include <stdio.h>

typedef enum {
    SCENE_REASON_COLD,
    SCENE_REASON_MAP_SPACE,
    SCENE_REASON_PENDING,
    SCENE_REASON_SWITCH_OFF,
    SCENE_REASON_COUNT
} SceneReason;

static DWORD last_static_continue_invalidate;
static int scene_cache_hits;
static int scene_cache_misses;
static int scene_cache_last_build_ms;
static int scene_last_background_ms;
static int scene_last_static_map_ms;
static int scene_last_overlay_ms;
static int scene_last_blit_ms;
static int static_base_presented_current;
static int static_base_presentable;
static int static_base_complete;
static int static_base_fully_current;
static int no_safe_frames;
static SceneReason scene_last_reason = SCENE_REASON_COLD;

static const char *scene_reason_name(SceneReason reason) {
    static const char *names[SCENE_REASON_COUNT] = {
        "cold", "map-space compose", "static pending", "static switch off"
    };
    return reason >= 0 && reason < SCENE_REASON_COUNT ? names[reason] : "unknown";
}

static int scene_wind_lod(MapLayout layout) {
    return wind_render_lod_bucket_for_tile_size(layout.tile_size);
}

static int scene_step_ms(DWORD start, int *slot, const char *name) {
    int elapsed = (int)(GetTickCount() - start);
    if (slot) *slot = elapsed;
    profiler_record_spike_phase(PROFILER_SPIKE_STATIC_CACHE, name, elapsed);
    return elapsed;
}

static void scene_reset_step_ms(void) {
    scene_last_background_ms = 0;
    scene_last_static_map_ms = 0;
    scene_last_overlay_ms = 0;
    scene_last_blit_ms = 0;
}

static void draw_visible_wind(HDC hdc, RECT client, MapLayout layout,
                              const RenderSnapshot *snapshot) {
    int lod;
    if (!map_display_policy_shows_wind(display_mode)) return;
    lod = scene_wind_lod(layout);
    if (render_static_physical_overlay_cache_wind_ready(snapshot, lod)) {
        render_static_physical_overlay_cache_present_wind(
            hdc, client, layout, snapshot, lod);
    }
}

static int static_continue_interval_ms(void) {
    if (auto_run && world_generated && speed_index >= SPEED_COUNT - 1 &&
        (simulation_worker_presentation_throttled() || simulation_worker_overloaded())) {
        return simulation_worker_overloaded() ? 3000 : 1500;
    }
    if (auto_run && world_generated && speed_index >= SPEED_COUNT - 1) return 250;
    return 33;
}

void render_static_scene_draw(HDC hdc, RECT client, MapLayout layout,
                              const RenderSnapshot *snapshot) {
    DWORD start = GetTickCount();
    DWORD step_start;
    int ready;
    scene_reset_step_ms();
    if (!profiling_switch_enabled(PROFILING_SWITCH_STATIC_SCENE)) {
        scene_last_reason = SCENE_REASON_SWITCH_OFF;
    } else {
        scene_last_reason = SCENE_REASON_MAP_SPACE;
    }

    step_start = GetTickCount();
    render_ocean_decoration_draw_background(hdc, client, layout, snapshot);
    scene_step_ms(step_start, &scene_last_background_ms,
                  "Static scene ocean background");

    step_start = GetTickCount();
    draw_cached_static_map_nonblocking(hdc, client, layout);
    scene_step_ms(step_start, &scene_last_static_map_ms,
                  "Static scene map-space layers");

    step_start = GetTickCount();
    draw_visible_wind(hdc, client, layout, snapshot);
    scene_step_ms(step_start, &scene_last_overlay_ms,
                  "Static scene map-space overlays");

    scene_cache_last_build_ms = (int)(GetTickCount() - start);
    profiler_record_spike_phase(PROFILER_SPIKE_STATIC_CACHE,
                                "Static scene map-space compose",
                                scene_cache_last_build_ms);
    static_base_presented_current =
        render_static_map_cache_presented_current();
    static_base_complete = render_static_map_cache_presented_boundary_safe();
    static_base_fully_current =
        render_static_map_cache_presented_fully_current();
    static_base_presentable = static_base_complete;
    ready = static_base_complete && static_base_fully_current &&
            !render_static_map_cache_needs_work();
    if (ready) scene_cache_hits++;
    else {
        scene_cache_misses++;
        scene_last_reason = SCENE_REASON_PENDING;
        if (!static_base_complete) no_safe_frames++;
    }
}

void render_static_scene_request_continue(HWND hwnd, int continue_static_work) {
    DWORD now;
    int interval;
    profiler_note_static_cache_state(render_static_map_cache_needs_work(),
                                     static_base_presented_current,
                                     static_base_presentable);
    if (!profiling_switch_enabled(PROFILING_SWITCH_STATIC_SCENE)) return;
    if (world_generated && !static_base_fully_current &&
        render_static_map_cache_needs_work())
        continue_static_work = 1;
    if (!continue_static_work) return;
    now = GetTickCount();
    interval = static_base_presentable ? static_continue_interval_ms() : 16;
    if (!auto_run || (int)(now - last_static_continue_invalidate) >= interval) {
        last_static_continue_invalidate = now;
        ui_invalidate_map_viewport(hwnd);
    }
}

int render_static_scene_presented_current(void) { return static_base_presented_current; }
int render_static_scene_presentable(void) { return static_base_presentable; }
int render_static_scene_defer_safe(RECT client, MapLayout layout,
                                   const RenderSnapshot *snapshot) {
    (void)client;
    (void)layout;
    return snapshot && snapshot->world_generated && static_base_complete;
}
int render_static_scene_complete(void) { return static_base_complete; }
int render_static_scene_fully_current(void) { return static_base_fully_current; }
int render_static_scene_no_safe_frames(void) { return no_safe_frames; }
int render_static_scene_stale_safe_age_ms(void) { return 0; }
int render_scene_cache_hits(void) { return scene_cache_hits; }
int render_scene_cache_misses(void) { return scene_cache_misses; }
int render_scene_cache_last_build_ms(void) { return scene_cache_last_build_ms; }
int render_scene_cache_last_reason_code(void) { return (int)scene_last_reason; }
const char *render_scene_cache_last_reason(void) {
    return scene_reason_name(scene_last_reason);
}
int render_scene_cache_deferred_reuses(void) { return 0; }
int render_scene_cache_viewport_rebuilds(void) { return 0; }

RenderLayerCacheMemory render_static_scene_memory(void) {
    RenderLayerCacheMemory empty = {0};
    return empty;
}

void render_static_scene_debug_times(int *background_ms, int *static_map_ms,
                                     int *overlay_ms, int *publish_ms,
                                     int *blit_ms) {
    if (background_ms) *background_ms = scene_last_background_ms;
    if (static_map_ms) *static_map_ms = scene_last_static_map_ms;
    if (overlay_ms) *overlay_ms = scene_last_overlay_ms;
    if (publish_ms) *publish_ms = 0;
    if (blit_ms) *blit_ms = scene_last_blit_ms;
}

const char *render_static_scene_status_summary(void) {
    static char text[192];
    snprintf(text, sizeof(text),
             "code %d / current %d / presentable %d / safe %d / full %d / no-safe %d / stale 0 / work %d / bg %d map %d ov %d pub 0 blit %d",
             (int)scene_last_reason, static_base_presented_current,
             static_base_presentable, static_base_complete,
             static_base_fully_current, no_safe_frames,
             render_static_map_cache_needs_work(), scene_last_background_ms,
             scene_last_static_map_ms, scene_last_overlay_ms,
             scene_last_blit_ms);
    return text;
}

void render_static_scene_reset_debug(void) {
    scene_cache_hits = 0;
    scene_cache_misses = 0;
    scene_cache_last_build_ms = 0;
    scene_reset_step_ms();
    static_base_presented_current = 0;
    static_base_presentable = 0;
    static_base_complete = 0;
    static_base_fully_current = 0;
    no_safe_frames = 0;
    scene_last_reason = SCENE_REASON_COLD;
}

void render_static_scene_invalidate_cache(void) {
    static_base_presented_current = 0;
    static_base_presentable = 0;
    static_base_complete = 0;
    static_base_fully_current = 0;
    scene_last_reason = SCENE_REASON_COLD;
}
