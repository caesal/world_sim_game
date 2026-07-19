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
#include "render/panel_view_model_cache.h"
#include "render/panel_map_speed_badge.h"
#include "render/plague_visual.h"
#include "render/profiling_switches.h"
#include "render/render_context.h"
#include "render/render_dynamic_overlay_cache.h"
#include "render/render_layer_cache.h"
#include "render/render_partial_ui.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "render/render_transient_ui.h"
#include "render/worldgen_progress_overlay.h"
#include "core/worldgen_progress.h"
#include "sim/simulation_worker.h"
#include "ui/ui_invalidation.h"
#include "ui/ui_theme.h"
#include <stdio.h>
static LayerCache ui_cache;
static LayerCache window_backbuffer;
static DWORD last_full_map_paint_tick;
static int last_full_paint_had_progress_overlay;
static int window_backbuffer_legend_collapsed = -1, window_backbuffer_language = -1;
static int window_backbuffer_has_transients;
#define record_render_phase(start, category, name) profiler_record_spike_phase((category), (name), profiler_elapsed_ms_since_us(start))
#define record_render_subphase(start, category, subphase, name) profiler_record_render_subphase((subphase), (category), (name), profiler_elapsed_ms_since_us(start))
static void draw_legacy_overlay_nonblocking(HDC hdc, RECT client, MapLayout layout) {
    if (profiling_switch_enabled(PROFILING_SWITCH_HIGHLIGHT)) draw_country_highlight(hdc, client, layout);
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
    }
    if (render_partial_ui_stale_indicator_needed())
        render_partial_ui_draw_stale_indicator(hdc, client);
}

static int render_input_waiting(void) { return 0; }
static int blit_cached_window(HDC hdc, int width, int height) {
    long long start;
    if (window_backbuffer_has_transients || render_transient_ui_has_visible(0) ||
        !window_backbuffer.dc || window_backbuffer.width != width || window_backbuffer.height != height ||
        window_backbuffer.display != display_mode || window_backbuffer.side_w != side_panel_w ||
        window_backbuffer_legend_collapsed != map_legend_collapsed || window_backbuffer_language != ui_language) return 0;
    start = profiler_now_us();
    BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY);
    record_render_subphase(start, PROFILER_SPIKE_PRESENTATION,
                           PROFILER_RENDER_SUB_BACKBUFFER, "Cached window blit");
    return 1;
}

static int static_presentation_safe_for_defer(RECT client, MapLayout layout, const RenderSnapshot *snapshot) { return render_static_scene_defer_safe(client, layout, snapshot); }

static int should_defer_presentation_map_paint(DWORD now, RECT client, MapLayout layout, const RenderSnapshot *snapshot) {
    (void)now; (void)client; (void)layout; (void)snapshot;
    return 0;
}

static void draw_deferred_over_cached_scene(HDC hdc, RECT client, const RenderSnapshot *snapshot, int draw_ui) {
    MapLayout layout = get_map_layout(client); long long phase_start;
    if (profiling_switch_enabled(PROFILING_SWITCH_DIPLOMACY_ANIMATION) && snapshot && snapshot->world_generated) {
        phase_start = profiler_now_us();
        if (render_static_scene_presentable()) diplomacy_map_anim_consume_events(snapshot); else diplomacy_map_anim_delay_for_snapshot(snapshot);
        draw_diplomacy_map_animations(hdc, client, layout, snapshot); record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Diplomacy animation");
    }
    if (!draw_ui) return;
    phase_start = profiler_now_us(); draw_top_bar(hdc, client); draw_bottom_bar(hdc, client); draw_map_frame_overlay(hdc, client);
    panel_map_draw_actual_speed_badge(hdc, client, game_loop_actual_ms_per_month());
    if (render_partial_ui_stale_indicator_needed())
        render_partial_ui_draw_stale_indicator(hdc, client);
    record_render_subphase(phase_start, PROFILER_SPIKE_PRESENTATION, PROFILER_RENDER_SUB_TOP_BOTTOM_BAR, "Deferred top/bottom");
    phase_start = profiler_now_us(); panel_view_model_cache_draw(hdc, client);
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
        route_exact = render_dynamic_route_overlay_draw(
            hdc, client, layout, snapshot, display_mode, selected_civ,
            side_panel_w, map_interaction_preview);
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
        if (profiling_switch_enabled(PROFILING_SWITCH_CITY_OVERLAY)) {
            render_dynamic_city_overlay_draw(hdc, client, layout, snapshot,
                                             display_mode, side_panel_w,
                                             map_interaction_preview);
        }
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
    int ui_only, continue_static_work;
    int announcement_activated, transients_handled = 0;
    int deferred_for_input = 0, progress_active;
    DWORD now = GetTickCount();

    GetClientRect(hwnd, &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    snapshot = render_snapshot_acquire();
    render_context_begin(snapshot);
    continue_static_work = render_static_map_cache_needs_work();
    worldgen_progress_get(&progress); progress_active = progress.active || load_progress_active();
    announcement_activated = render_transient_ui_prepare(hdc, client, progress_active);
    ui_only = !announcement_activated &&
              render_partial_ui_can_paint(client, ps.rcPaint, snapshot) &&
              !diplomacy_map_anim_requires_dynamic_paint(snapshot);
    if (ui_only) {
        long long phase_start = profiler_now_us();
        render_partial_ui_draw(hdc, client, ps.rcPaint);
        record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Partial UI paint");
    } else if (!progress_active && !last_full_paint_had_progress_overlay &&
                !render_partial_ui_map_data_dirty() &&
               static_presentation_safe_for_defer(client, get_map_layout(client), snapshot) &&
               render_input_waiting() &&
               blit_cached_window(hdc, width, height)) {
        deferred_for_input = 1;
        draw_deferred_over_cached_scene(hdc, client, snapshot, 0);
    } else if (!progress_active &&
               should_defer_presentation_map_paint(now, client, get_map_layout(client), snapshot) &&
               blit_cached_window(hdc, width, height)) {
        deferred_for_input = 1;
        draw_deferred_over_cached_scene(hdc, client, snapshot, 1);
    } else if (render_layer_cache_ensure(hdc, &window_backbuffer, client, get_map_layout(client), side_panel_w, display_mode)) {
        long long phase_start = profiler_now_us();
        render_world(window_backbuffer.dc, client);
        GdiFlush();
        window_backbuffer_has_transients = 0;
        window_backbuffer_legend_collapsed = map_legend_collapsed; window_backbuffer_language = ui_language;
        record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Render world");
        if (render_transient_ui_has_visible(progress_active)) {
            render_transient_ui_draw_full(window_backbuffer.dc, client, progress_active);
            window_backbuffer_has_transients = 1;
        }
        phase_start = profiler_now_us();
        BitBlt(hdc, 0, 0, width, height, window_backbuffer.dc, 0, 0, SRCCOPY);
        record_render_subphase(phase_start, PROFILER_SPIKE_PRESENTATION,
                               PROFILER_RENDER_SUB_BACKBUFFER, "Backbuffer blit");
        last_full_map_paint_tick = now;
        transients_handled = 1;
    } else {
        long long phase_start = profiler_now_us();
        render_world(hdc, client);
        GdiFlush();
        record_render_phase(phase_start, PROFILER_SPIKE_PRESENTATION, "Render world");
        last_full_map_paint_tick = now;
    }
    if (!ui_only && !transients_handled)
        render_transient_ui_draw_full(hdc, client, progress_active);
    if (!ui_only) { if (!deferred_for_input) last_full_paint_had_progress_overlay = progress_active; continue_static_work = render_static_map_cache_needs_work(); }
    if (deferred_for_input) continue_static_work = 1;
    render_context_end();
    render_snapshot_release(snapshot);
    profiler_record_render_ms(profiler_elapsed_ms_since_us(render_start));
    EndPaint(hwnd, &ps);
    render_static_scene_request_continue(hwnd, continue_static_work);
}

const char *render_scene_cache_reason_summary(void) {
    static char text[160];
    snprintf(text, sizeof(text), "%s / %s", render_static_scene_status_summary(),
             render_dynamic_overlay_status_summary());
    return text; }
void render_scene_cache_reset_debug(void) {
    render_static_scene_reset_debug();
    render_dynamic_overlay_reset_debug();
}
