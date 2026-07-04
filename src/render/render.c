#include "render_internal.h"
#include "core/dirty_flags.h"
#include "core/load_progress.h"
#include "core/plague_perf.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_keys.h"
#include "game/game_loop.h"
#include "render/cartography_layers.h"
#include "render/country_target_arrow.h"
#include "render/diplomacy_map_anim.h"
#include "render/load_progress_overlay.h"
#include "render/map_highlight.h"
#include "render/pause_menu_render.h"
#include "render/panel_view_model_cache.h"
#include "render/panel_map_speed_badge.h"
#include "render/plague_visual.h"
#include "render/profiling_switches.h"
#include "render/render_context.h"
#include "render/render_layer_cache.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "render/top_notifications.h"
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
static LayerCache route_overlay_cache;
static LayerCache city_overlay_cache;
static int route_overlay_cache_hits, route_overlay_cache_misses, route_overlay_exact_rebuilds, route_overlay_preview_reuses;
static int city_overlay_cache_hits, city_overlay_cache_misses, city_overlay_exact_rebuilds, city_overlay_preview_reuses;
static int city_overlay_last_rebuild_ms, city_overlay_peak_rebuild_ms, city_overlay_last_blit_ms, city_overlay_peak_blit_ms;
static DWORD last_city_overlay_rebuild_tick;
static int city_overlay_cached_visual_revision = -1;
static char overlay_last_reason_text[48] = "cold";
static DWORD last_full_map_paint_tick;
static int last_full_paint_had_progress_overlay;
static int window_backbuffer_legend_collapsed = -1, window_backbuffer_language = -1;
#define record_render_phase(start, category, name) profiler_record_spike_phase((category), (name), profiler_elapsed_ms_since_us(start))
#define record_render_subphase(start, category, subphase, name) profiler_record_render_subphase((subphase), (category), (name), profiler_elapsed_ms_since_us(start))
static void draw_legacy_overlay_nonblocking(HDC hdc, RECT client, MapLayout layout) {
    if (profiling_switch_enabled(PROFILING_SWITCH_HIGHLIGHT)) draw_country_highlight(hdc, client, layout);
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
    if (display_mode == DISPLAY_REGIONS) {
        key = render_layer_mix_key(key, snapshot ? snapshot->regions_revision : 0);
    }
    return render_layer_mix_key(key, display_mode);
}
static void note_city_overlay_blit(long long start) {
    city_overlay_last_blit_ms = profiler_elapsed_ms_since_us(start);
    if (city_overlay_last_blit_ms > city_overlay_peak_blit_ms) city_overlay_peak_blit_ms = city_overlay_last_blit_ms;
    profiler_record_render_subphase(PROFILER_RENDER_SUB_CITY_OVERLAY, PROFILER_SPIKE_CITY_OVERLAY,
                                    "City overlay blit", city_overlay_last_blit_ms);
}
static int draw_city_overlay_presentation(HDC hdc, RECT client, MapLayout layout, const RenderSnapshot *snapshot) {
    unsigned int key = city_overlay_key(snapshot);
    long long start;
    if (render_layer_cache_matches(&city_overlay_cache, client, layout, key, display_mode)) {
        city_overlay_cache_hits++;
        start = profiler_now_us();
        render_layer_cache_transparent_viewport(hdc, client, &city_overlay_cache);
        note_city_overlay_blit(start);
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
    start = profiler_now_us();
    { RECT clear = get_map_viewport_rect(client); fill_rect(city_overlay_cache.dc, clear, RGB(255, 0, 255)); }
    draw_cities(city_overlay_cache.dc, layout);
    city_overlay_last_rebuild_ms = profiler_elapsed_ms_since_us(start);
    if (city_overlay_last_rebuild_ms > city_overlay_peak_rebuild_ms) city_overlay_peak_rebuild_ms = city_overlay_last_rebuild_ms;
    profiler_record_render_subphase(PROFILER_RENDER_SUB_CITY_OVERLAY, PROFILER_SPIKE_CITY_OVERLAY,
                                    "City overlay rebuild", city_overlay_last_rebuild_ms);
    city_overlay_cache.key = key;
    city_overlay_cache.valid = 1;
    city_overlay_cached_visual_revision = snapshot ? snapshot->city_visual_revision : 0;
    last_city_overlay_rebuild_tick = GetTickCount();
    city_overlay_exact_rebuilds++;
    snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text), "city exact rebuild");
    start = profiler_now_us();
    render_layer_cache_transparent_viewport(hdc, client, &city_overlay_cache);
    note_city_overlay_blit(start);
    return 1;
}

static void draw_stale_ui_indicator(HDC hdc, RECT client) {
    RECT reset = get_reset_view_button_rect(client), year_box = {client.right / 2 - 112, 9, client.right / 2 + 112, 50};
    RECT badge = {reset.left - 136, reset.top + 3, reset.left - 8, reset.bottom - 3};
    if (badge.left < year_box.right + 12) badge.left = year_box.right + 12;
    if (badge.right - badge.left < 96) return;
    fill_rect_alpha(hdc, badge, RGB(42, 48, 54), 210); draw_center_text(hdc, badge, tr("Updating data", "数据更新中"), RGB(218, 226, 232));
}

static int stale_ui_indicator_needed(void) {
    if (render_snapshot_age_ms() <= 500) return 0;
    if (simulation_worker_pending_months() > 0 || simulation_worker_visual_backlog() > 0 || simulation_worker_presentation_throttled() || simulation_worker_overloaded()) return 1;
    if (dirty_render_terrain() || dirty_render_political() || dirty_render_coast() ||
        dirty_render_hydrology() || dirty_render_borders() || dirty_render_maritime() ||
        dirty_render_labels() || dirty_render_plague()) return 1;
    return render_static_map_cache_needs_work() || !render_static_scene_complete();
}

static void draw_legacy_ui_nonblocking(HDC hdc, RECT client, int draw_legend) {
    MapLayout layout = get_map_layout(client);
    long long start;
    if (render_layer_cache_ensure(hdc, &ui_cache, client, layout, side_panel_w, display_mode)) {
        BitBlt(ui_cache.dc, 0, 0, ui_cache.width, ui_cache.height, hdc, 0, 0, SRCCOPY);
        start = profiler_now_us();
        draw_top_bar(ui_cache.dc, client);
        draw_bottom_bar(ui_cache.dc, client);
        record_render_subphase(start, PROFILER_SPIKE_PRESENTATION,
                               PROFILER_RENDER_SUB_TOP_BOTTOM_BAR, "Top/bottom bar");
        draw_map_frame_overlay(ui_cache.dc, client);
        if (draw_legend && profiling_switch_enabled(PROFILING_SWITCH_MAP_LEGEND)) {
            start = profiler_now_us();
            draw_map_legend(ui_cache.dc, client);
            record_render_subphase(start, PROFILER_SPIKE_PRESENTATION,
                                   PROFILER_RENDER_SUB_MAP_LEGEND, "Map legend");
        }
        start = profiler_now_us();
        panel_view_model_cache_draw(ui_cache.dc, client);
        record_render_subphase(start, PROFILER_SPIKE_PRESENTATION,
                               PROFILER_RENDER_SUB_SIDE_PANEL, "Side panel");
        draw_top_notifications(ui_cache.dc, client);
        if (pause_menu_open) draw_pause_menu_overlay(ui_cache.dc, client);
        start = profiler_now_us();
        BitBlt(hdc, 0, 0, ui_cache.width, ui_cache.height, ui_cache.dc, 0, 0, SRCCOPY);
        record_render_subphase(start, PROFILER_SPIKE_PRESENTATION,
                               PROFILER_RENDER_SUB_BACKBUFFER, "UI cache blit");
    } else {
        start = profiler_now_us();
        draw_top_bar(hdc, client);
        draw_bottom_bar(hdc, client);
        record_render_subphase(start, PROFILER_SPIKE_PRESENTATION,
                               PROFILER_RENDER_SUB_TOP_BOTTOM_BAR, "Top/bottom bar");
        draw_map_frame_overlay(hdc, client);
        if (draw_legend && profiling_switch_enabled(PROFILING_SWITCH_MAP_LEGEND)) {
            start = profiler_now_us();
            draw_map_legend(hdc, client);
            record_render_subphase(start, PROFILER_SPIKE_PRESENTATION,
                                   PROFILER_RENDER_SUB_MAP_LEGEND, "Map legend");
        }
        start = profiler_now_us();
        panel_view_model_cache_draw(hdc, client);
        record_render_subphase(start, PROFILER_SPIKE_PRESENTATION,
                               PROFILER_RENDER_SUB_SIDE_PANEL, "Side panel");
        draw_top_notifications(hdc, client);
        if (pause_menu_open) draw_pause_menu_overlay(hdc, client);
    }
    if (stale_ui_indicator_needed()) draw_stale_ui_indicator(hdc, client);
}

static int rects_intersect(RECT a, RECT b) {
    RECT out;
    return IntersectRect(&out, &a, &b);
}

static int rect_contains_rect(RECT o, RECT i) { return i.left >= o.left && i.top >= o.top && i.right <= o.right && i.bottom <= o.bottom && i.right > i.left && i.bottom > i.top; }
static RECT speed_badge_dirty_rect(RECT client) { RECT r = panel_map_actual_speed_badge_rect(client); InflateRect(&r, 2, 2); return r; }
static int paint_is_speed_badge_update(RECT client, RECT paint) { RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H, client.right, client.bottom}, badge = speed_badge_dirty_rect(client); return rect_contains_rect(badge, paint) || (rects_intersect(paint, bottom) && rects_intersect(paint, badge) && paint.left >= client.left && paint.right <= client.right && paint.top >= badge.top && paint.bottom <= client.bottom); }

static int paint_is_ui_chrome_only(RECT client, RECT paint) {
    RECT top = {client.left, client.top, client.right, TOP_BAR_H}, bottom = {client.left, client.bottom - BOTTOM_BAR_H, client.right, client.bottom};
    RECT side = get_side_panel_draw_rect(client), handle = get_side_panel_handle_rect(client);
    RECT handle_dirty = get_side_panel_handle_dirty_rect(client);
    if (!side_panel_collapsed && handle.left < side.left) side.left = handle.left;
    return rect_contains_rect(top, paint) || rect_contains_rect(bottom, paint) ||
           rect_contains_rect(side, paint) || rect_contains_rect(handle, paint) ||
           rect_contains_rect(handle_dirty, paint) || paint_is_speed_badge_update(client, paint);
}

static void draw_partial_ui(HDC hdc, RECT client, RECT paint) {
    RECT top = {client.left, client.top, client.right, TOP_BAR_H};
    RECT bottom = {client.left, client.bottom - BOTTOM_BAR_H, client.right, client.bottom};
    RECT panel = get_side_panel_draw_rect(client);
    RECT badge = speed_badge_dirty_rect(client);
    if (rects_intersect(paint, top)) {
        draw_top_bar(hdc, client);
        if (stale_ui_indicator_needed()) draw_stale_ui_indicator(hdc, client);
    }
    if (rects_intersect(paint, bottom)) draw_bottom_bar(hdc, client);
    if (rects_intersect(paint, badge)) panel_map_draw_actual_speed_badge(hdc, client, game_loop_actual_ms_per_month());
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
    long long start;
    if (!window_backbuffer.dc || window_backbuffer.width != width || window_backbuffer.height != height ||
        window_backbuffer.display != display_mode || window_backbuffer.side_w != side_panel_w ||
        window_backbuffer_legend_collapsed != map_legend_collapsed || window_backbuffer_language != ui_language) return 0;
    start = profiler_now_us();
    BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY);
    record_render_subphase(start, PROFILER_SPIKE_PRESENTATION,
                           PROFILER_RENDER_SUB_BACKBUFFER, "Cached window blit");
    return 1;
}

static int static_presentation_safe_for_defer(RECT client, MapLayout layout,
                                              const RenderSnapshot *snapshot) { return render_static_scene_defer_safe(client, layout, snapshot); }

static int should_defer_presentation_map_paint(DWORD now, RECT client, MapLayout layout,
                                               const RenderSnapshot *snapshot) {
    (void)now;
    (void)client;
    (void)layout;
    (void)snapshot;
    return map_interaction_preview;
}

static void draw_deferred_ui_over_cached_scene(HDC hdc, RECT client) {
    long long phase_start = profiler_now_us(); draw_top_bar(hdc, client); draw_bottom_bar(hdc, client); draw_map_frame_overlay(hdc, client);
    panel_map_draw_actual_speed_badge(hdc, client, game_loop_actual_ms_per_month());
    if (stale_ui_indicator_needed()) draw_stale_ui_indicator(hdc, client);
    record_render_subphase(phase_start, PROFILER_SPIKE_PRESENTATION, PROFILER_RENDER_SUB_TOP_BOTTOM_BAR, "Deferred top/bottom");
    phase_start = profiler_now_us(); panel_view_model_cache_draw(hdc, client); draw_top_notifications(hdc, client);
    record_render_subphase(phase_start, PROFILER_SPIKE_PRESENTATION, PROFILER_RENDER_SUB_SIDE_PANEL, "Deferred side panel");
}

static void render_world(HDC hdc, RECT client) {
    MapLayout layout = get_map_layout(client);
    const RenderSnapshot *snapshot = render_context_snapshot();
    int snapshot_world_ready = snapshot && snapshot->world_generated, static_ready;
    long long phase_start;
    WorldGenProgress progress;

    worldgen_progress_get(&progress);
    if (load_progress_active()) {
        RECT viewport = get_map_viewport_rect(client);
        fill_rect(hdc, client, RGB(13, 17, 21));
        draw_legacy_ui_nonblocking(hdc, client, 1);
        fill_rect(hdc, viewport, RGB(13, 17, 21));
        draw_load_progress_overlay(hdc, client);
        return;
    }
    if (progress.active) {
        RECT viewport = get_map_viewport_rect(client);
        fill_rect(hdc, client, RGB(13, 17, 21));
        draw_legacy_ui_nonblocking(hdc, client, 1);
        fill_rect(hdc, viewport, RGB(13, 17, 21));
        draw_worldgen_progress_overlay(hdc, client);
        return;
    }
    phase_start = profiler_now_us();
    render_static_scene_draw(hdc, client, layout, snapshot);
    record_render_subphase(phase_start, PROFILER_SPIKE_STATIC_CACHE,
                           PROFILER_RENDER_SUB_STATIC_SCENE, "Static scene draw");
    static_ready = render_static_scene_presented_current();
    if (snapshot_world_ready) {
        int route_exact;
        phase_start = profiler_now_us();
        route_exact = draw_route_overlay_presentation(hdc, client, layout, snapshot);
        record_render_subphase(phase_start, PROFILER_SPIKE_ROUTE_MARITIME,
                               PROFILER_RENDER_SUB_ROUTE_OVERLAY, "Route overlay");
        if (route_exact && (!dirty_render_maritime() ||
            snapshot->lanes_revision == render_snapshot_lanes_revision_key())) {
            dirty_clear_render_maritime();
        }
        if (!map_interaction_preview && plague_perf_visuals_allowed()) {
            phase_start = profiler_now_us();
            draw_plague_region_overlay(hdc, client, layout);
            record_render_subphase(phase_start, PROFILER_SPIKE_PRESENTATION,
                                   PROFILER_RENDER_SUB_HIGHLIGHT_OVERLAY, "Plague overlay");
            dirty_clear_render_plague();
        } else if (!plague_perf_visuals_allowed()) {
            plague_perf_note_visual_skipped(1);
            dirty_clear_render_plague();
        }
        phase_start = profiler_now_us();
        draw_legacy_overlay_nonblocking(hdc, client, layout);
        record_render_subphase(phase_start, PROFILER_SPIKE_PRESENTATION,
                               PROFILER_RENDER_SUB_HIGHLIGHT_OVERLAY, "Highlight overlay");
        if (profiling_switch_enabled(PROFILING_SWITCH_CITY_OVERLAY)) draw_city_overlay_presentation(hdc, client, layout, snapshot);
        {
            int labels_dirty = dirty_render_labels();
            if (profiling_switch_enabled(PROFILING_SWITCH_MAP_LABELS)) {
                long long label_start = profiler_now_us();
                draw_map_labels(hdc, client, layout);
                profiler_record_render_subphase(display_mode == DISPLAY_ALLIANCE ?
                                                PROFILER_RENDER_SUB_ALLIANCE_LABELS :
                                                PROFILER_RENDER_SUB_COUNTRY_LABELS,
                                                PROFILER_SPIKE_LABEL_CACHE,
                                                display_mode == DISPLAY_ALLIANCE ?
                                                "Alliance labels" : "Map labels",
                                                profiler_elapsed_ms_since_us(label_start));
            }
            if (labels_dirty && !map_interaction_preview) profiler_add_render_rebuild(PROFILER_RENDER_LABEL);
            if (!map_interaction_preview) dirty_clear_render_labels();
        }
        if (profiling_switch_enabled(PROFILING_SWITCH_DIPLOMACY_ANIMATION)) {
            int diplomacy_scene_ready = static_ready || render_static_scene_presentable();
            phase_start = profiler_now_us();
            if (diplomacy_scene_ready) diplomacy_map_anim_consume_events(snapshot);
            else diplomacy_map_anim_delay_for_snapshot(snapshot);
            draw_diplomacy_map_animations(hdc, client, layout, snapshot);
            record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Diplomacy animation");
        }
        phase_start = profiler_now_us();
        draw_selected_tile(hdc, layout);
        draw_country_target_arrow(hdc, client, layout, snapshot);
        record_render_subphase(phase_start, PROFILER_SPIKE_PRESENTATION,
                               PROFILER_RENDER_SUB_HIGHLIGHT_OVERLAY, "Selection overlay");
    } else {
        dirty_clear_render_maritime();
        dirty_clear_render_plague();
        dirty_clear_render_labels();
    }
    phase_start = profiler_now_us();
    draw_legacy_ui_nonblocking(hdc, client, 1);
    draw_worldgen_progress_overlay(hdc, client);
    draw_load_progress_overlay(hdc, client);
    record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "UI chrome/panel");
}

void paint_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client; WorldGenProgress progress;
    const RenderSnapshot *snapshot;
    long long render_start = profiler_now_us();
    int width;
    int height;
    int ui_only;
    int diplomacy_dynamic;
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
    diplomacy_dynamic = profiling_switch_enabled(PROFILING_SWITCH_DIPLOMACY_ANIMATION) && diplomacy_map_anim_requires_dynamic_paint(snapshot);
    if (diplomacy_dynamic && ui_only) { RECT viewport = get_map_viewport_rect(client); InvalidateRect(hwnd, &viewport, FALSE); ui_only = 0; diplomacy_map_anim_note_cached_paint_blocked(); }
    if (diplomacy_dynamic && !ui_only && !progress_active && (render_input_waiting() || should_defer_presentation_map_paint(now, client, get_map_layout(client), snapshot))) diplomacy_map_anim_note_cached_paint_blocked();
    if (ui_only) {
        long long phase_start = profiler_now_us();
        draw_partial_ui(hdc, client, ps.rcPaint);
        record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Partial UI paint");
    } else if (!progress_active && !last_full_paint_had_progress_overlay &&
               static_presentation_safe_for_defer(client, get_map_layout(client), snapshot) &&
               render_input_waiting() &&
               !diplomacy_dynamic &&
               blit_cached_window(hdc, width, height)) {
        deferred_for_input = 1;
    } else if (!progress_active &&
               should_defer_presentation_map_paint(now, client, get_map_layout(client), snapshot) &&
               !diplomacy_dynamic &&
               blit_cached_window(hdc, width, height)) {
        deferred_for_input = 1;
        draw_deferred_ui_over_cached_scene(hdc, client);
    } else if (render_layer_cache_ensure(hdc, &window_backbuffer, client, get_map_layout(client), side_panel_w, display_mode)) {
        long long phase_start = profiler_now_us();
        render_world(window_backbuffer.dc, client);
        window_backbuffer_legend_collapsed = map_legend_collapsed; window_backbuffer_language = ui_language;
        record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Render world");
        phase_start = profiler_now_us();
        color_picker_draw(window_backbuffer.dc, client);
        record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Color picker draw");
        phase_start = profiler_now_us();
        BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY);
        record_render_subphase(phase_start, PROFILER_SPIKE_PRESENTATION,
                               PROFILER_RENDER_SUB_BACKBUFFER, "Backbuffer blit");
        last_full_map_paint_tick = now;
    } else {
        long long phase_start = profiler_now_us();
        render_world(hdc, client);
        record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Render world");
        phase_start = profiler_now_us();
        color_picker_draw(hdc, client);
        record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Color picker draw");
        last_full_map_paint_tick = now;
    }
    if (!ui_only) { if (!deferred_for_input) last_full_paint_had_progress_overlay = progress_active; continue_static_work = render_static_map_cache_needs_work(); }
    if (deferred_for_input) continue_static_work = 1;
    render_context_end();
    render_snapshot_release(snapshot);
    profiler_record_render_ms(profiler_elapsed_ms_since_us(render_start));
    EndPaint(hwnd, &ps);
    render_static_scene_request_continue(hwnd, continue_static_work);
}

int render_city_overlay_cache_hits(void) { return city_overlay_cache_hits; } int render_city_overlay_cache_misses(void) { return city_overlay_cache_misses; }
int render_city_overlay_exact_rebuilds(void) { return city_overlay_exact_rebuilds; } int render_city_overlay_preview_reuses(void) { return city_overlay_preview_reuses; }
int render_city_overlay_last_rebuild_ms(void) { return city_overlay_last_rebuild_ms; } int render_city_overlay_peak_rebuild_ms(void) { return city_overlay_peak_rebuild_ms; }
int render_city_overlay_last_blit_ms(void) { return city_overlay_last_blit_ms; } int render_city_overlay_peak_blit_ms(void) { return city_overlay_peak_blit_ms; }
const char *render_overlay_cache_last_reason(void) { return overlay_last_reason_text; }
const char *render_scene_cache_reason_summary(void) {
    static char text[160];
    snprintf(text, sizeof(text), "%s / r %d/%d/%d/%d c %d/%d/%d/%d %s", render_static_scene_status_summary(),
             route_overlay_cache_hits, route_overlay_cache_misses, route_overlay_exact_rebuilds,
             route_overlay_preview_reuses, city_overlay_cache_hits, city_overlay_cache_misses,
             city_overlay_exact_rebuilds, city_overlay_preview_reuses, overlay_last_reason_text);
    return text;
}
void render_scene_cache_reset_debug(void) {
    render_static_scene_reset_debug(); route_overlay_cache_hits = route_overlay_cache_misses = 0;
    route_overlay_exact_rebuilds = route_overlay_preview_reuses = city_overlay_cache_hits = city_overlay_cache_misses = 0; city_overlay_exact_rebuilds = city_overlay_preview_reuses = 0;
    city_overlay_last_rebuild_ms = city_overlay_peak_rebuild_ms = 0; city_overlay_last_blit_ms = city_overlay_peak_blit_ms = 0;
    last_city_overlay_rebuild_tick = GetTickCount(); city_overlay_cached_visual_revision = -1; snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text), "reset");
}
