#include "render/render_static_scene.h"

#include "core/dirty_flags.h"
#include "core/profiler.h"
#include "render/profiling_switches.h"
#include "render/map_display_policy.h"
#include "render/map_ownership_surface.h"
#include "render/render_ocean_decoration.h"
#include "render/render_layer_cache.h"
#include "render/render_static_map_cache.h"
#include "render/snapshot_map_layers.h"
#include "sim/simulation_worker.h"
#include "ui/ui_invalidation.h"

#include <stdio.h>

typedef enum {
    SCENE_REASON_COLD,
    SCENE_REASON_HIT,
    SCENE_REASON_REBUILD,
    SCENE_REASON_PENDING,
    SCENE_REASON_DEFERRED,
    SCENE_REASON_SWITCH_OFF,
    SCENE_REASON_DIRECT,
    SCENE_REASON_COUNT
} SceneReason;

static LayerCache viewport_static_published_cache;
static LayerCache viewport_static_scratch_cache;
static DWORD last_static_continue_invalidate;
static DWORD last_viewport_static_rebuild_tick;
static int scene_cache_hits, scene_cache_misses, scene_cache_last_build_ms;
static int static_base_presented_current, static_base_presentable;
static int static_base_complete, static_base_fully_current, no_safe_frames;
static int viewport_static_published_current, viewport_static_published_safe, viewport_static_published_full;
static unsigned int viewport_static_published_boundary_key;
static DWORD stale_safe_start_tick;
static int stale_safe_age_ms;
static SceneReason scene_last_reason = SCENE_REASON_COLD;

static const char *scene_reason_name(SceneReason reason) {
    static const char *names[SCENE_REASON_COUNT] = {
        "cold", "viewport-static hit", "viewport-static rebuild",
        "static rebuild pending", "viewport-static deferred",
        "static switch off", "direct static draw"
    };
    return reason >= 0 && reason < SCENE_REASON_COUNT ? names[reason] : "unknown";
}

static int presentation_ownership_revision(const RenderSnapshot *snapshot) {
    if (snapshot && world_generated && map_w == snapshot->map_w && map_h == snapshot->map_h &&
        map_display_policy_requires_fill_layer(display_mode) &&
        display_mode != DISPLAY_REGIONS) return map_ownership_surface_live_revision();
    return snapshot ? map_ownership_surface_snapshot_revision(snapshot) : 0;
}

static int display_uses_civ_visual_static_key(void) {
    return display_mode == DISPLAY_POLITICAL || display_mode == DISPLAY_ALLIANCE ||
           display_mode == DISPLAY_ALL;
}

static unsigned int static_base_key(RECT client, MapLayout layout,
                                    const RenderSnapshot *snapshot) {
    unsigned int key = render_layer_layout_key(client, layout, side_panel_w, display_mode);
    key = render_layer_mix_key(key, snapshot ? snapshot->map_w : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->map_h : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->terrain_revision : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->coast_revision : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->hydrology_revision : 0);
    key = render_layer_mix_key(key, presentation_ownership_revision(snapshot));
    if (display_mode == DISPLAY_ALLIANCE) key = render_layer_mix_key(key, dirty_revision_alliance());
    if (display_uses_civ_visual_static_key()) {
        key = render_layer_mix_key(key, snapshot ? snapshot->civ_visual_revision : 0);
    }
    return key;
}

static unsigned int static_base_boundary_key(RECT client, MapLayout layout,
                                             const RenderSnapshot *snapshot) {
    unsigned int key = render_layer_layout_key(client, layout, side_panel_w, display_mode);
    key = render_layer_mix_key(key, snapshot ? snapshot->map_w : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->map_h : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->terrain_revision : 0);
    key = render_layer_mix_key(key, presentation_ownership_revision(snapshot));
    if (display_mode == DISPLAY_ALLIANCE) key = render_layer_mix_key(key, dirty_revision_alliance());
    if (display_uses_civ_visual_static_key()) {
        key = render_layer_mix_key(key, snapshot ? snapshot->civ_visual_revision : 0);
    }
    return key;
}

static int max_speed_static_defer(void) {
    return auto_run && world_generated && speed_index >= SPEED_COUNT - 1 &&
           !map_interaction_preview;
}

static int static_continue_interval_ms(void) {
    if (auto_run && world_generated && speed_index >= SPEED_COUNT - 1 &&
        (simulation_worker_presentation_throttled() || simulation_worker_overloaded())) {
        return simulation_worker_overloaded() ? 3000 : 1500;
    }
    if (auto_run && world_generated && speed_index >= SPEED_COUNT - 1) return 250;
    return 33;
}

static void blit_presentable(HDC hdc, RECT client, const LayerCache *cache,
                             int current, int safe, int full, SceneReason reason) {
    scene_last_reason = reason;
    static_base_presentable = 1;
    static_base_presented_current = current;
    static_base_complete = safe;
    static_base_fully_current = full;
    render_layer_cache_blit_viewport(hdc, client, cache);
}

static int publish_scratch(HDC hdc, RECT client, MapLayout layout,
                           unsigned int key, unsigned int boundary_key,
                           int current, int safe, int full) {
    if (!safe || !viewport_static_scratch_cache.valid) return 0;
    if (!render_layer_cache_ensure(hdc, &viewport_static_published_cache, client, layout,
                                   side_panel_w, display_mode)) return 0;
    BitBlt(viewport_static_published_cache.dc, 0, 0,
           viewport_static_published_cache.width, viewport_static_published_cache.height,
           viewport_static_scratch_cache.dc, 0, 0, SRCCOPY);
    viewport_static_published_cache.key = key;
    viewport_static_published_cache.valid = 1;
    viewport_static_published_boundary_key = boundary_key;
    viewport_static_published_current = current;
    viewport_static_published_safe = safe;
    viewport_static_published_full = full;
    last_viewport_static_rebuild_tick = GetTickCount();
    return 1;
}

void render_static_scene_draw(HDC hdc, RECT client, MapLayout layout,
                              const RenderSnapshot *snapshot) {
    unsigned int key = static_base_key(client, layout, snapshot);
    unsigned int boundary_key = static_base_boundary_key(client, layout, snapshot);
    DWORD now = GetTickCount();
    DWORD start;
    int presentable = viewport_static_published_safe &&
                      render_layer_cache_preview_presentable(&viewport_static_published_cache,
                                                             client, display_mode);
    int exact = render_layer_cache_matches(&viewport_static_published_cache, client, layout,
                                           key, display_mode);
    int boundary_exact = presentable && viewport_static_published_boundary_key == boundary_key;
    if (presentable && !boundary_exact) {
        if (!stale_safe_start_tick) stale_safe_start_tick = now;
        stale_safe_age_ms = (int)(now - stale_safe_start_tick);
    } else {
        stale_safe_start_tick = 0;
        stale_safe_age_ms = 0;
    }
    static_base_presented_current = 0;
    static_base_presentable = presentable;
    static_base_complete = boundary_exact && viewport_static_published_safe;
    static_base_fully_current = exact && viewport_static_published_full;
    if (exact && viewport_static_published_current && viewport_static_published_safe) {
        scene_cache_hits++;
        blit_presentable(hdc, client, &viewport_static_published_cache, 1, 1,
                         viewport_static_published_full, SCENE_REASON_HIT);
        return;
    }
    if (!profiling_switch_enabled(PROFILING_SWITCH_STATIC_SCENE)) {
        scene_last_reason = SCENE_REASON_SWITCH_OFF;
        if (presentable) {
            blit_presentable(hdc, client, &viewport_static_published_cache,
                             exact && viewport_static_published_current,
                             boundary_exact && viewport_static_published_safe,
                             exact && viewport_static_published_full,
                             SCENE_REASON_SWITCH_OFF);
        }
        else fill_rect(hdc, get_map_viewport_rect(client), RGB(28, 34, 38));
        return;
    }
    if (presentable && boundary_exact && max_speed_static_defer() &&
        (int)(now - last_viewport_static_rebuild_tick) < 900) {
        scene_cache_hits++;
        blit_presentable(hdc, client, &viewport_static_published_cache, 0, 1, 0,
                         SCENE_REASON_DEFERRED);
        return;
    }
    scene_cache_misses++;
    start = GetTickCount();
    if (render_layer_cache_ensure(hdc, &viewport_static_scratch_cache, client, layout,
                                  side_panel_w, display_mode)) {
        int needs_work;
        int safe;
        int full;
        int current;
        render_ocean_decoration_draw_background(viewport_static_scratch_cache.dc, client, layout, snapshot);
        draw_cached_static_map_nonblocking(viewport_static_scratch_cache.dc, client, layout);
        render_ocean_decoration_draw_overlay(viewport_static_scratch_cache.dc, client, layout, snapshot);
        scene_cache_last_build_ms = (int)(GetTickCount() - start);
        profiler_record_spike_phase(PROFILER_SPIKE_STATIC_CACHE, "Static scene", scene_cache_last_build_ms);
        needs_work = render_static_map_cache_needs_work();
        safe = render_static_map_cache_presented_boundary_safe();
        full = render_static_map_cache_presented_fully_current();
        current = full && !needs_work && render_static_map_cache_presented_current();
        viewport_static_scratch_cache.key = key;
        if (publish_scratch(hdc, client, layout, key, boundary_key, current, safe, full)) {
            scene_last_reason = current ? SCENE_REASON_REBUILD : SCENE_REASON_PENDING;
            blit_presentable(hdc, client, &viewport_static_published_cache,
                             current, 1, full, scene_last_reason);
        } else if (presentable) {
            blit_presentable(hdc, client, &viewport_static_published_cache, 0,
                             boundary_exact && viewport_static_published_safe, 0,
                             SCENE_REASON_PENDING);
        } else {
            static_base_presentable = 0;
            static_base_presented_current = 0;
            static_base_complete = safe;
            static_base_fully_current = full;
            scene_last_reason = SCENE_REASON_PENDING;
            if (!safe) no_safe_frames++;
            render_layer_cache_blit_viewport(hdc, client, &viewport_static_scratch_cache);
        }
    } else {
        render_ocean_decoration_draw_background(hdc, client, layout, snapshot);
        draw_cached_static_map_nonblocking(hdc, client, layout);
        render_ocean_decoration_draw_overlay(hdc, client, layout, snapshot);
        scene_cache_last_build_ms = (int)(GetTickCount() - start);
        profiler_record_spike_phase(PROFILER_SPIKE_STATIC_CACHE, "Static scene direct", scene_cache_last_build_ms);
        static_base_presented_current = render_static_map_cache_presented_current();
        static_base_complete = render_static_map_cache_presented_boundary_safe();
        static_base_fully_current = render_static_map_cache_presented_fully_current();
        static_base_presentable = 0;
        if (!static_base_complete) no_safe_frames++;
        scene_last_reason = SCENE_REASON_DIRECT;
    }
}

void render_static_scene_request_continue(HWND hwnd, int continue_static_work) {
    DWORD now;
    int interval;
    profiler_note_static_cache_state(render_static_map_cache_needs_work(),
                                     static_base_presented_current,
                                     static_base_presentable);
    if (!continue_static_work || !profiling_switch_enabled(PROFILING_SWITCH_STATIC_SCENE)) return;
    now = GetTickCount();
    if (!render_static_scene_presentable()) interval = 16;
    else if (!render_static_map_cache_ownership_current()) interval = 16;
    else if (max_speed_static_defer()) interval = static_continue_interval_ms();
    else if (!render_static_scene_fully_current()) interval = 33;
    else interval = render_static_scene_complete() ? static_continue_interval_ms() : 100;
    if (!auto_run || (int)(now - last_static_continue_invalidate) >= interval) {
        last_static_continue_invalidate = now;
        ui_invalidate_map_viewport(hwnd);
    }
}

int render_static_scene_presented_current(void) { return static_base_presented_current; }
int render_static_scene_presentable(void) { return static_base_presentable; }
int render_static_scene_defer_safe(RECT client, MapLayout layout, const RenderSnapshot *snapshot) {
    unsigned int boundary_key = static_base_boundary_key(client, layout, snapshot);
    return viewport_static_published_safe &&
           viewport_static_published_boundary_key == boundary_key &&
           render_layer_cache_preview_presentable(&viewport_static_published_cache,
                                                  client, display_mode);
}
int render_static_scene_complete(void) { return static_base_complete; }
int render_static_scene_fully_current(void) { return static_base_fully_current; }
int render_static_scene_no_safe_frames(void) { return no_safe_frames; }
int render_static_scene_stale_safe_age_ms(void) { return stale_safe_age_ms; }
int render_scene_cache_hits(void) { return scene_cache_hits; }
int render_scene_cache_misses(void) { return scene_cache_misses; }
int render_scene_cache_last_build_ms(void) { return scene_cache_last_build_ms; }
int render_scene_cache_last_reason_code(void) { return (int)scene_last_reason; }
const char *render_scene_cache_last_reason(void) { return scene_reason_name(scene_last_reason); }
const char *render_static_scene_status_summary(void) {
    static char text[128];
    snprintf(text, sizeof(text), "code %d / current %d / presentable %d / safe %d / full %d / no-safe %d / stale %d / work %d",
             (int)scene_last_reason, static_base_presented_current,
             static_base_presentable, static_base_complete,
             static_base_fully_current, no_safe_frames, stale_safe_age_ms,
             render_static_map_cache_needs_work());
    return text;
}
void render_static_scene_reset_debug(void) {
    scene_cache_hits = scene_cache_misses = scene_cache_last_build_ms = 0;
    static_base_presented_current = static_base_presentable = static_base_complete = 0;
    static_base_fully_current = no_safe_frames = 0;
    stale_safe_start_tick = 0;
    stale_safe_age_ms = 0;
    scene_last_reason = SCENE_REASON_COLD;
    last_viewport_static_rebuild_tick = GetTickCount();
}

void render_static_scene_invalidate_cache(void) {
    viewport_static_published_cache.valid = 0;
    viewport_static_scratch_cache.valid = 0;
    static_base_presented_current = static_base_presentable = static_base_complete = 0;
    static_base_fully_current = 0;
    stale_safe_start_tick = 0;
    stale_safe_age_ms = 0;
    scene_last_reason = SCENE_REASON_COLD;
}
