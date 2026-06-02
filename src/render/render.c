#include "render_internal.h"
#include "core/dirty_flags.h"
#include "core/load_progress.h"
#include "core/plague_perf.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_keys.h"
#include "render/cartography_layers.h"
#include "render/diplomacy_map_anim.h"
#include "render/load_progress_overlay.h"
#include "render/map_highlight.h"
#include "render/pause_menu_render.h"
#include "render/panel_view_model_cache.h"
#include "render/plague_visual.h"
#include "render/render_context.h"
#include "render/render_layer_cache.h"
#include "render/render_static_map_cache.h"
#include "render/worldgen_progress_overlay.h"
#include "core/worldgen_progress.h"
#include "sim/simulation_worker.h"
#include "ui/color_picker.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_theme.h"
#include <stdio.h>

static LayerCache ui_cache;
static LayerCache side_panel_cache;
static LayerCache window_backbuffer;
static LayerCache viewport_static_cache;
static LayerCache route_overlay_cache;
static LayerCache city_overlay_cache;
static int scene_cache_hits;
static int scene_cache_misses;
static int scene_cache_last_build_ms;
static int route_overlay_cache_hits, route_overlay_cache_misses;
static int city_overlay_cache_hits, city_overlay_cache_misses;
static int route_overlay_exact_rebuilds, route_overlay_preview_reuses, city_overlay_exact_rebuilds, city_overlay_preview_reuses;
static char scene_cache_last_reason_text[64] = "cold";
static char overlay_last_reason_text[48] = "cold";
static DWORD last_static_continue_invalidate;
static DWORD last_full_map_paint_tick;
static int static_base_presented_current, last_full_paint_had_progress_overlay;

static void draw_legacy_overlay_nonblocking(HDC hdc, RECT client, MapLayout layout) {
    draw_country_highlight(hdc, client, layout);
}

static int draw_preview_layer(HDC hdc, RECT client, MapLayout layout, LayerCache *cache,
                              int *counter, const char *reuse, const char *skip) {
    if (render_layer_cache_preview_presentable(cache, client, display_mode)) {
        (*counter)++;
        snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text), "%s", reuse);
        render_layer_cache_transparent_map(hdc, client, layout, cache);
    } else snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text), "%s", skip);
    return 0;
}

static unsigned int static_base_key(RECT client, MapLayout layout, const RenderSnapshot *snapshot) {
    unsigned int key = render_layer_layout_key(client, layout, side_panel_w, display_mode);
    key = render_layer_mix_key(key, snapshot ? snapshot->map_w : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->map_h : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->terrain_revision : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->coast_revision : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->hydrology_revision : 0);
    return render_layer_mix_key(key, snapshot ? snapshot->regions_revision : 0);
}

static void draw_static_base_presentation(HDC hdc, RECT client, MapLayout layout,
                                          const RenderSnapshot *snapshot) {
    unsigned int key = static_base_key(client, layout, snapshot);
    DWORD start;
    static_base_presented_current = 0;
    if (render_layer_cache_matches(&viewport_static_cache, client, layout, key, display_mode)) {
        scene_cache_hits++;
        snprintf(scene_cache_last_reason_text, sizeof(scene_cache_last_reason_text), "viewport-static hit");
        static_base_presented_current = 1;
        render_layer_cache_blit_viewport(hdc, client, &viewport_static_cache);
        return;
    }
    scene_cache_misses++;
    start = GetTickCount();
    if (render_layer_cache_ensure(hdc, &viewport_static_cache, client, layout, side_panel_w, display_mode)) {
        draw_cached_static_map_nonblocking(viewport_static_cache.dc, client, layout);
        scene_cache_last_build_ms = (int)(GetTickCount() - start);
        viewport_static_cache.key = key;
        viewport_static_cache.valid = !render_static_map_cache_needs_work();
        static_base_presented_current = viewport_static_cache.valid &&
                                        render_static_map_cache_presented_current();
        snprintf(scene_cache_last_reason_text, sizeof(scene_cache_last_reason_text),
                 viewport_static_cache.valid ? "viewport-static rebuild" : "static rebuild pending");
        render_layer_cache_blit_viewport(hdc, client, &viewport_static_cache);
    } else {
        draw_cached_static_map_nonblocking(hdc, client, layout);
        scene_cache_last_build_ms = (int)(GetTickCount() - start);
        static_base_presented_current = render_static_map_cache_presented_current();
        snprintf(scene_cache_last_reason_text, sizeof(scene_cache_last_reason_text), "direct static draw");
    }
}

static unsigned int route_overlay_key(const RenderSnapshot *snapshot) {
    unsigned int key = 2166136261u;
    key = render_layer_mix_key(key, snapshot ? snapshot->lanes_revision : 0);
    key = render_layer_mix_key(key, selected_civ);
    return render_layer_mix_key(key, display_mode);
}

static int draw_route_overlay_presentation(HDC hdc, RECT client, MapLayout layout,
                                           const RenderSnapshot *snapshot) {
    unsigned int key;
    if (!snapshot) return 0;
    key = route_overlay_key(snapshot);
    if (map_interaction_preview) {
        if (render_layer_cache_matches(&route_overlay_cache, client, layout, key, display_mode)) {
            route_overlay_preview_reuses++;
            snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text),
                     "%s", "route preview exact");
            render_layer_cache_transparent_viewport(hdc, client, &route_overlay_cache);
            return 1;
        }
        snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text),
                 "%s", "route preview skip dirty");
        return 0;
    }
    if (display_mode == DISPLAY_ROUTE_POTENTIAL || (plague_perf_visuals_allowed() &&
         (snapshot->plague_active || plague_visual_infected_lane_count() > 0))) {
        draw_maritime_routes(hdc, client, layout);
        return 1;
    }
    if (render_layer_cache_matches(&route_overlay_cache, client, layout, key, display_mode)) {
        route_overlay_cache_hits++;
        render_layer_cache_transparent_viewport(hdc, client, &route_overlay_cache);
        return 1;
    }
    route_overlay_cache_misses++;
    if (!render_layer_cache_ensure(hdc, &route_overlay_cache, client, layout, side_panel_w, display_mode)) {
        draw_maritime_routes(hdc, client, layout);
        return 1;
    }
    render_layer_cache_clear_transparent(&route_overlay_cache);
    draw_maritime_routes(route_overlay_cache.dc, client, layout);
    route_overlay_cache.key = key;
    route_overlay_cache.valid = 1;
    route_overlay_exact_rebuilds++;
    snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text), "route exact rebuild");
    render_layer_cache_transparent_viewport(hdc, client, &route_overlay_cache);
    return 1;
}

static unsigned int city_overlay_key(const RenderSnapshot *snapshot) {
    unsigned int key = 2166136261u;
    key = render_layer_mix_key(key, snapshot ? snapshot->map_w : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->map_h : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->city_visual_revision : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->cities_revision : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->regions_revision : 0);
    return render_layer_mix_key(key, display_mode);
}

static int draw_city_overlay_presentation(HDC hdc, RECT client, MapLayout layout,
                                          const RenderSnapshot *snapshot) {
    unsigned int key = city_overlay_key(snapshot);
    if (render_layer_cache_matches(&city_overlay_cache, client, layout, key, display_mode)) {
        city_overlay_cache_hits++;
        render_layer_cache_transparent_viewport(hdc, client, &city_overlay_cache);
        return 1;
    }
    if (map_interaction_preview) {
        return draw_preview_layer(hdc, client, layout, &city_overlay_cache,
                                  &city_overlay_preview_reuses,
                                  "city preview reuse", "city preview skip");
    }
    city_overlay_cache_misses++;
    if (!render_layer_cache_ensure(hdc, &city_overlay_cache, client, layout, side_panel_w, display_mode)) {
        draw_cities(hdc, layout);
        return 1;
    }
    render_layer_cache_clear_transparent(&city_overlay_cache);
    draw_cities(city_overlay_cache.dc, layout);
    city_overlay_cache.key = key;
    city_overlay_cache.valid = 1;
    city_overlay_exact_rebuilds++;
    snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text), "city exact rebuild");
    render_layer_cache_transparent_viewport(hdc, client, &city_overlay_cache);
    return 1;
}

static void draw_non_plague_map_scene(HDC hdc, RECT client, MapLayout layout,
                                      const RenderSnapshot *snapshot) {
    draw_static_base_presentation(hdc, client, layout, snapshot);
}

static void draw_stale_ui_indicator(HDC hdc, RECT client) {
    RECT reset = get_reset_view_button_rect(client);
    RECT year_box = {client.right / 2 - 112, 9, client.right / 2 + 112, 50};
    RECT badge = {reset.left - 136, reset.top + 3, reset.left - 8, reset.bottom - 3};
    if (badge.left < year_box.right + 12) badge.left = year_box.right + 12;
    if (badge.right - badge.left < 96) return;
    fill_rect_alpha(hdc, badge, RGB(42, 48, 54), 210);
    draw_center_text(hdc, badge, tr("Updating data", "数据更新中"), RGB(218, 226, 232));
}

static void draw_legacy_ui_nonblocking(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);
    if (render_layer_cache_ensure(hdc, &ui_cache, client, layout, side_panel_w, display_mode)) {
        BitBlt(ui_cache.dc, 0, 0, ui_cache.width, ui_cache.height, hdc, 0, 0, SRCCOPY);
        draw_top_bar(ui_cache.dc, client);
        draw_bottom_bar(ui_cache.dc, client);
        draw_map_frame_overlay(ui_cache.dc, client);
        draw_map_legend(ui_cache.dc, client);
        panel_view_model_cache_draw(ui_cache.dc, client);
        if (pause_menu_open) draw_pause_menu_overlay(ui_cache.dc, client);
        BitBlt(hdc, 0, 0, ui_cache.width, ui_cache.height, ui_cache.dc, 0, 0, SRCCOPY);
    } else {
        draw_top_bar(hdc, client);
        draw_bottom_bar(hdc, client);
        draw_map_frame_overlay(hdc, client);
        draw_map_legend(hdc, client);
        panel_view_model_cache_draw(hdc, client);
        if (pause_menu_open) draw_pause_menu_overlay(hdc, client);
    }
    if (render_snapshot_age_ms() > 500) draw_stale_ui_indicator(hdc, client);
}

static int rects_intersect(RECT a, RECT b) {
    RECT out;
    return IntersectRect(&out, &a, &b);
}

static int rect_contains_rect(RECT o, RECT i) { return i.left >= o.left && i.top >= o.top && i.right <= o.right && i.bottom <= o.bottom && i.right > i.left && i.bottom > i.top; }

static int paint_is_ui_chrome_only(RECT client, RECT paint) {
    RECT top = {client.left, client.top, client.right, TOP_BAR_H}, bottom = {client.left, client.bottom - BOTTOM_BAR_H, client.right, client.bottom};
    RECT side = get_side_panel_draw_rect(client), handle = get_side_panel_handle_rect(client);
    RECT handle_dirty = get_side_panel_handle_dirty_rect(client);
    if (!side_panel_collapsed && handle.left < side.left) side.left = handle.left;
    return rect_contains_rect(top, paint) || rect_contains_rect(bottom, paint) ||
           rect_contains_rect(side, paint) || rect_contains_rect(handle, paint) ||
           rect_contains_rect(handle_dirty, paint);
}

static void draw_partial_ui(HDC hdc, RECT client, RECT paint) {
    RECT top = {client.left, client.top, client.right, TOP_BAR_H};
    RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H, client.right, client.bottom};
    RECT panel = get_side_panel_draw_rect(client);
    if (rects_intersect(paint, top)) {
        draw_top_bar(hdc, client);
        if (render_snapshot_age_ms() > 500) draw_stale_ui_indicator(hdc, client);
    }
    if (rects_intersect(paint, bottom)) draw_bottom_bar(hdc, client);
    if (rects_intersect(paint, panel)) {
        if (side_panel_collapsed) {
            panel_view_model_cache_draw(hdc, client);
        } else if (render_layer_cache_ensure(hdc, &side_panel_cache, client, get_map_layout(client), side_panel_w, display_mode)) {
            fill_rect(side_panel_cache.dc, panel, ui_theme_color(UI_COLOR_PANEL));
            panel_view_model_cache_draw(side_panel_cache.dc, client);
            BitBlt(hdc, panel.left, panel.top, panel.right - panel.left, panel.bottom - panel.top,
                   side_panel_cache.dc, panel.left, panel.top, SRCCOPY);
        } else {
            panel_view_model_cache_draw(hdc, client);
        }
    }
}

static int can_paint_ui_only(RECT client, RECT paint) {
    RECT viewport = get_map_viewport_rect(client);
    WorldGenProgress progress;
    worldgen_progress_get(&progress);
    if (color_picker_active() || pause_menu_open || progress.active || load_progress_active()) return 0;
    if (paint_is_ui_chrome_only(client, paint)) return 1;
    return !rects_intersect(paint, viewport);
}

static int render_input_waiting(void) { return (HIWORD(GetQueueStatus(QS_INPUT | QS_SENDMESSAGE)) & (QS_INPUT | QS_SENDMESSAGE)) != 0; }
static int blit_cached_window(HDC hdc, int width, int height) {
    if (!window_backbuffer.dc || window_backbuffer.width != width || window_backbuffer.height != height ||
        window_backbuffer.display != display_mode || window_backbuffer.side_w != side_panel_w) return 0;
    BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY); return 1;
}

static int should_defer_presentation_map_paint(DWORD now) {
    if (!auto_run || !world_generated || speed_index < SPEED_COUNT - 1) return 0;
    if (map_interaction_preview) return 0;
    if (!simulation_worker_presentation_throttled() && !simulation_worker_overloaded()) return 0;
    return last_full_map_paint_tick > 0 && (int)(now - last_full_map_paint_tick) < 1000;
}

static int static_continue_interval_ms(void) {
    if (auto_run && world_generated && speed_index >= SPEED_COUNT - 1 &&
        (simulation_worker_presentation_throttled() || simulation_worker_overloaded())) {
        return 500;
    }
    if (auto_run && world_generated && speed_index >= SPEED_COUNT - 1) return 250;
    return 33;
}

static void render_world(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);
    const RenderSnapshot *snapshot = render_context_snapshot();
    int snapshot_world_ready = snapshot && snapshot->world_generated;
    int static_ready;
    WorldGenProgress progress;

    worldgen_progress_get(&progress);
    if (load_progress_active()) {
        RECT viewport = get_map_viewport_rect(client);
        fill_rect(hdc, client, RGB(13, 17, 21));
        draw_legacy_ui_nonblocking(hdc, client);
        fill_rect(hdc, viewport, RGB(13, 17, 21));
        draw_load_progress_overlay(hdc, client);
        return;
    }
    if (progress.active) {
        RECT viewport = get_map_viewport_rect(client);
        fill_rect(hdc, client, RGB(13, 17, 21));
        draw_legacy_ui_nonblocking(hdc, client);
        fill_rect(hdc, viewport, RGB(13, 17, 21));
        draw_worldgen_progress_overlay(hdc, client);
        return;
    }
    draw_non_plague_map_scene(hdc, client, layout, snapshot);
    static_ready = static_base_presented_current;
    if (snapshot_world_ready) {
        int route_exact = draw_route_overlay_presentation(hdc, client, layout, snapshot);
        if (route_exact && (!dirty_render_maritime() ||
            snapshot->lanes_revision == render_snapshot_lanes_revision_key())) {
            dirty_clear_render_maritime();
        }
        if (!map_interaction_preview && plague_perf_visuals_allowed()) {
            draw_plague_region_overlay(hdc, client, layout);
            dirty_clear_render_plague();
        } else if (!plague_perf_visuals_allowed()) {
            plague_perf_note_visual_skipped(1);
            dirty_clear_render_plague();
        }
        draw_legacy_overlay_nonblocking(hdc, client, layout);
        draw_city_overlay_presentation(hdc, client, layout, snapshot);
        {
            int labels_dirty = dirty_render_labels();
            draw_map_labels(hdc, client, layout);
            if (labels_dirty && !map_interaction_preview) profiler_add_render_rebuild(PROFILER_RENDER_LABEL);
            if (!map_interaction_preview) dirty_clear_render_labels();
        }
        if (static_ready) diplomacy_map_anim_consume_events(snapshot);
        else diplomacy_map_anim_delay_for_snapshot(snapshot);
        draw_diplomacy_map_animations(hdc, client, layout);
        draw_selected_tile(hdc, layout);
    } else {
        dirty_clear_render_maritime();
        dirty_clear_render_plague();
        dirty_clear_render_labels();
    }
    draw_legacy_ui_nonblocking(hdc, client);
    draw_worldgen_progress_overlay(hdc, client);
    draw_load_progress_overlay(hdc, client);
}

void paint_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client; WorldGenProgress progress;
    const RenderSnapshot *snapshot;
    DWORD render_start = GetTickCount();
    int width;
    int height;
    int ui_only;
    int continue_static_work;
    int deferred_for_input = 0, progress_active;
    DWORD now = GetTickCount();

    GetClientRect(hwnd, &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    snapshot = render_snapshot_acquire();
    render_context_begin(snapshot);
    ui_only = can_paint_ui_only(client, ps.rcPaint);
    continue_static_work = render_static_map_cache_needs_work();
    worldgen_progress_get(&progress); progress_active = progress.active || load_progress_active();
    if (ui_only) {
        draw_partial_ui(hdc, client, ps.rcPaint);
    } else if (!progress_active && !last_full_paint_had_progress_overlay && render_input_waiting() && blit_cached_window(hdc, width, height)) {
        deferred_for_input = 1;
    } else if (!progress_active && should_defer_presentation_map_paint(now) &&
               blit_cached_window(hdc, width, height)) {
        deferred_for_input = 1;
        draw_legacy_ui_nonblocking(hdc, client);
    } else if (render_layer_cache_ensure(hdc, &window_backbuffer, client, get_map_layout(client), side_panel_w, display_mode)) {
        render_world(window_backbuffer.dc, client);
        color_picker_draw(window_backbuffer.dc, client);
        BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY);
        last_full_map_paint_tick = now;
    } else {
        render_world(hdc, client);
        color_picker_draw(hdc, client);
        last_full_map_paint_tick = now;
    }
    if (!ui_only) { if (!deferred_for_input) last_full_paint_had_progress_overlay = progress_active; continue_static_work = render_static_map_cache_needs_work(); }
    if (deferred_for_input) continue_static_work = 1;
    render_context_end();
    render_snapshot_release(snapshot);
    profiler_record_render_ms((int)(GetTickCount() - render_start));
    EndPaint(hwnd, &ps);
    if (continue_static_work) {
        DWORD now = GetTickCount();
        int interval = static_continue_interval_ms();
        if ((int)(now - last_static_continue_invalidate) >= interval) {
            last_static_continue_invalidate = now;
            ui_invalidate_map_viewport(hwnd);
        }
    }
}

void render_paint_side_panel_now(HWND hwnd) {
    RECT client, panel, handle_dirty, paint; WorldGenProgress progress;
    const RenderSnapshot *snapshot; HDC hdc; int saved;
    DWORD start = GetTickCount();
    if (!hwnd) return;
    GetClientRect(hwnd, &client);
    worldgen_progress_get(&progress);
    if (color_picker_active() || pause_menu_open || progress.active || load_progress_active()) return;
    panel = get_side_panel_draw_rect(client); handle_dirty = get_side_panel_handle_dirty_rect(client);
    UnionRect(&paint, &panel, &handle_dirty);
    if (paint.right <= paint.left || paint.bottom <= paint.top) return;
    hdc = GetDC(hwnd); if (!hdc) return;
    snapshot = render_snapshot_acquire(); render_context_begin(snapshot); saved = SaveDC(hdc);
    IntersectClipRect(hdc, paint.left, paint.top, paint.right, paint.bottom);
    draw_partial_ui(hdc, client, paint);
    RestoreDC(hdc, saved);
    render_context_end(); render_snapshot_release(snapshot); ReleaseDC(hwnd, hdc); ValidateRect(hwnd, &paint);
    profiler_record_render_ms((int)(GetTickCount() - start));
}

int render_scene_cache_hits(void) { return scene_cache_hits; }
int render_scene_cache_misses(void) { return scene_cache_misses; }
int render_scene_cache_last_build_ms(void) { return scene_cache_last_build_ms; }
const char *render_scene_cache_last_reason(void) { return scene_cache_last_reason_text; }
int render_city_overlay_cache_hits(void) { return city_overlay_cache_hits; }
int render_city_overlay_cache_misses(void) { return city_overlay_cache_misses; }
int render_city_overlay_exact_rebuilds(void) { return city_overlay_exact_rebuilds; }
int render_city_overlay_preview_reuses(void) { return city_overlay_preview_reuses; }
const char *render_overlay_cache_last_reason(void) { return overlay_last_reason_text; }
const char *render_scene_cache_reason_summary(void) {
    static char text[160];
    snprintf(text, sizeof(text), "r %d/%d/%d/%d c %d/%d/%d/%d %s",
             route_overlay_cache_hits, route_overlay_cache_misses,
             route_overlay_exact_rebuilds, route_overlay_preview_reuses,
             city_overlay_cache_hits, city_overlay_cache_misses,
             city_overlay_exact_rebuilds, city_overlay_preview_reuses,
             overlay_last_reason_text);
    return text;
}
