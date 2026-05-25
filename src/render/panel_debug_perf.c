#include "render/panel_debug_perf.h"
#include "render/contour_paths.h"
#include "render/panel_debug_worldgen.h"
#include "render/plague_visual.h"
#include "render/panel_view_model_cache.h"
#include "render/map_labels.h"
#include "render/render.h"
#include "render/render_static_map_cache.h"
#include "render/sea_lane_render.h"
#include "render/render_context.h"
#include "render_panel_internal.h"
#include "core/dirty_flags.h"
#include "core/plague_perf.h"
#include "core/profiler.h"
#include "core/render_snapshot_civs.h"
#include "core/render_snapshot_profile.h"
#include "game/game_loop.h"
#include "sim/decision_snapshot.h"
#include "sim/sea_lanes.h"
#include "ui/ui_theme.h"
#include <stdio.h>

static COLORREF budget_color(int value, int budget) {
    if (budget <= 0 || value < budget * 7 / 10) return RGB(132, 188, 111);
    if (value <= budget) return RGB(218, 178, 78);
    return RGB(218, 92, 78);
}
static void perf_row(HDC hdc, UiCursor *cursor, const char *label, const char *value, COLORREF color) {
    char text[256];
    RECT rect;
    if (cursor->y > cursor->bottom - 20) return;
    snprintf(text, sizeof(text), "%s: %s", label, value);
    rect = (RECT){cursor->x, cursor->y, cursor->x + cursor->width, cursor->y + 20};
    draw_text_rect(hdc, rect, text, color, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    cursor->y += 21;
}
static void draw_phase_timing(HDC hdc, UiCursor *cursor, const RuntimeProfilerSnapshot *perf) {
    char text[160];
    ui_section(hdc, cursor, tr("Phase Timing", "阶段耗时"));
    snprintf(text, sizeof(text), "%s %d ms",
             perf->slowest_phase[0] ? perf->slowest_phase : "none", perf->slowest_phase_ms);
    perf_row(hdc, cursor, tr("Highest cost", "最高耗时"), text,
              budget_color(perf->slowest_phase_ms, perf->sim_budget_ms));
    snprintf(text, sizeof(text), "res %d / pop %d / exp %d / claim %d",
             perf->resource_ms, perf->population_ms, perf->expansion_ms, perf->claim_ms);
    perf_row(hdc, cursor, tr("Core phases", "核心阶段"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "dip %d / plague %d / war-cal %d",
             perf->diplomacy_ms, perf->plague_ms, perf->war_ms);
    perf_row(hdc, cursor, tr("Other phases", "其他阶段"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
}
void draw_debug_performance_panel(HDC hdc, UiCursor *cursor) {
    RuntimeProfilerSnapshot perf;
    char text[180];
    profiler_snapshot(&perf);
    ui_section(hdc, cursor, tr("Snapshot", "快照"));
    snprintf(text, sizeof(text), "age %d ms / rev #%u / publish %d ms",
             render_snapshot_age_ms(), render_snapshot_revision(), render_snapshot_last_publish_ms());
    perf_row(hdc, cursor, tr("Render snapshot", "渲染快照"), text,
              render_snapshot_age_ms() > 500 ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "skip %d / throttled %d / %s",
             render_snapshot_skipped_publish_count(), render_snapshot_throttled_publish_count(),
             render_snapshot_last_skip_reason());
    perf_row(hdc, cursor, tr("Publish", "发布"), text,
              render_snapshot_skipped_publish_count() > 0 ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    if (render_context_snapshot()) {
        snprintf(text, sizeof(text), "copied 0x%02X / skipped 0x%02X", render_context_snapshot()->sections_copied_mask, render_context_snapshot()->sections_skipped_mask);
        perf_row(hdc, cursor, tr("Section copy mask", "快照分区复制"), text,
                  ui_theme_color(UI_COLOR_TEXT_MUTED));
    }
    snprintf(text, sizeof(text), "total %d / wait %d / held %d ms",
             render_snapshot_profile_total_ms(), render_snapshot_profile_lock_wait_ms(),
             render_snapshot_profile_lock_held_ms());
    perf_row(hdc, cursor, tr("Snapshot lock", "快照锁"), text,
              render_snapshot_profile_lock_held_ms() > 30 ? RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "T%d C%d Ci%d R%d D%d L%d P%d E%d",
             render_snapshot_profile_section_ms(SNAPSHOT_PROFILE_TILES),
             render_snapshot_profile_section_ms(SNAPSHOT_PROFILE_CIVS),
             render_snapshot_profile_section_ms(SNAPSHOT_PROFILE_CITIES),
             render_snapshot_profile_section_ms(SNAPSHOT_PROFILE_REGIONS),
             render_snapshot_profile_section_ms(SNAPSHOT_PROFILE_DIPLOMACY),
             render_snapshot_profile_section_ms(SNAPSHOT_PROFILE_LANES),
             render_snapshot_profile_section_ms(SNAPSHOT_PROFILE_PLAGUE),
             render_snapshot_profile_section_ms(SNAPSHOT_PROFILE_EVENTS));
    perf_row(hdc, cursor, tr("Snapshot sections", "快照分区"), text,
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "raw %d / country %d / pop %d / decision %d / exp %d / sea %d / names %d",
             render_snapshot_profile_civ_phase_ms(SNAPSHOT_CIV_PROFILE_RAW),
             render_snapshot_profile_civ_phase_ms(SNAPSHOT_CIV_PROFILE_COUNTRY),
             render_snapshot_profile_civ_phase_ms(SNAPSHOT_CIV_PROFILE_POPULATION),
             render_snapshot_profile_civ_phase_ms(SNAPSHOT_CIV_PROFILE_DECISION),
             render_snapshot_profile_civ_phase_ms(SNAPSHOT_CIV_PROFILE_EXPANSION),
             render_snapshot_profile_civ_phase_ms(SNAPSHOT_CIV_PROFILE_MARITIME),
             render_snapshot_profile_civ_phase_ms(SNAPSHOT_CIV_PROFILE_NAMES));
    perf_row(hdc, cursor, tr("Civ copy phases", "文明复制阶段"), text,
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "valid %d / dirty %d / update %d civs %d ms",
             decision_snapshot_cache_valid_count(), decision_snapshot_cache_dirty_count(),
             decision_snapshot_cache_last_update_count(), decision_snapshot_cache_last_update_ms());
    perf_row(hdc, cursor, tr("Decision cache", "决策缓存"), text,
              decision_snapshot_cache_dirty_count() > 0 ? RGB(218, 178, 78) :
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "cached %d / stale %d / fallback %d",
             render_snapshot_civ_decision_cached_count(),
             render_snapshot_civ_decision_stale_count(),
             render_snapshot_civ_decision_fallback_count());
    perf_row(hdc, cursor, tr("Snapshot decisions", "快照决策"), text,
              render_snapshot_civ_decision_fallback_count() > 0 ? RGB(218, 178, 78) :
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    ui_section(hdc, cursor, tr("Simulation Clock", "模拟时钟"));
    snprintf(text, sizeof(text), "target 16 ms / avg %d / peak %d", perf.frame_avg_ms, perf.frame_peak_ms);
    perf_row(hdc, cursor, tr("Frame avg / peak", "帧均值 / 峰值"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%d ms/month, %.1f months/sec", perf.actual_ms_per_month,
             perf.actual_ms_per_month > 0 ? 1000.0 / perf.actual_ms_per_month : 0.0);
    perf_row(hdc, cursor, tr("Actual month time", "实际每月耗时"), text,
              perf.overloaded ? RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    ui_section(hdc, cursor, tr("Queue / Scheduler", "队列 / 调度器"));
    snprintf(text, sizeof(text), "%d ms used of %d ms budget", perf.sim_used_ms, perf.sim_budget_ms);
    perf_row(hdc, cursor, tr("Frame sim budget", "本帧模拟预算"), text,
              budget_color(perf.sim_used_ms, perf.sim_budget_ms));
    snprintf(text, sizeof(text), "pending %d / step %d ms", perf.pending_months, perf.scheduler_step_ms);
    perf_row(hdc, cursor, tr("Queue / step", "队列 / 单步"), text,
              perf.scheduler_step_over_budget ? RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "visual %d / coalesced %d / throttle %s",
             game_loop_presentation_backlog(), game_loop_visual_coalesced_months(),
             game_loop_presentation_throttled() ? "yes" : "no");
    perf_row(hdc, cursor, tr("Presentation", "展示背压"), text,
              game_loop_presentation_throttled() ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "months %d / map %s / flags 0x%02X",
             game_loop_last_completed_months(),
             game_loop_last_completed_month_map_redraw() ? "yes" : "no",
             game_loop_last_redraw_flags());
    perf_row(hdc, cursor, tr("Completed month", "Completed month"), text,
              game_loop_last_completed_month_map_redraw() ? RGB(218, 178, 78) :
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Map invalidation", "Map invalidation"),
              game_loop_last_map_redraw_reason(), ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Current Job", "当前任务"),
              perf.current_job[0] ? perf.current_job : "Idle", ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Worker", "模拟线程"), game_loop_worker_status(),
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    ui_section(hdc, cursor, tr("Performance", "性能"));
    perf_row(hdc, cursor, tr("Slow Call", "慢调用"),
              perf.last_slow_call[0] ? perf.last_slow_call : tr("None over 50 ms", "无超过 50ms"),
              perf.last_slow_call_ms > perf.sim_budget_ms ? RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Status", "状态"),
              perf.overloaded ? tr("Performance limited; backlog prevented", "性能受限，已防止积压") :
                                tr("Responsive", "响应正常"),
              perf.overloaded ? RGB(218, 92, 78) : RGB(132, 188, 111));
    draw_phase_timing(hdc, cursor, &perf);
    ui_section(hdc, cursor, tr("Render Cache", "渲染缓存"));
    snprintf(text, sizeof(text), "%d / %d ms", perf.render_avg_ms, perf.render_peak_ms);
    perf_row(hdc, cursor, tr("Render avg / peak", "渲染均值 / 峰值"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "terrain %d / political %d / borders %d / labels %d / gdi %d",
             perf.terrain_rebuild_count, perf.political_rebuild_count,
             perf.border_rebuild_count, perf.label_rebuild_count,
             perf.gdi_bitmap_recreate_count);
    perf_row(hdc, cursor, tr("Layer rebuilds", "图层重建"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "hit %d / miss %d / %d ms",
             render_scene_cache_hits(), render_scene_cache_misses(),
             render_scene_cache_last_build_ms());
    perf_row(hdc, cursor, tr("Scene cache", "场景缓存"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s / %s", render_scene_cache_last_reason(),
             render_scene_cache_reason_summary());
    perf_row(hdc, cursor, tr("Scene reason", "场景原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s / %s", render_static_map_cache_last_reason(),
             render_static_map_cache_reason_summary());
    perf_row(hdc, cursor, tr("Static reason", "静态原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "build %d ms / age %d ms / refresh %d",
             panel_view_model_cache_last_build_ms(), panel_view_model_cache_age_ms(),
             panel_view_model_cache_refresh_count());
    perf_row(hdc, cursor, tr("Side panel cache", "侧栏缓存"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s / %s", panel_view_model_cache_last_reason(),
             panel_view_model_cache_reason_summary());
    perf_row(hdc, cursor, tr("Panel reason", "侧栏原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "labels %d/%d / last %d ms / rebuild %d",
             map_label_cache_drawn_count(), map_label_cache_candidate_count(),
             map_label_cache_last_rebuild_ms(), map_label_cache_rebuild_count());
    perf_row(hdc, cursor, tr("Label layout", "标签布局"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s / %s", map_label_cache_last_reason(),
             map_label_cache_reason_summary());
    perf_row(hdc, cursor, tr("Label reason", "标签原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Label revisions", "Label revisions"),
              dirty_label_revision_summary(), ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "cities %d / ports %d",
             render_city_icons_drawn_last_frame(), render_port_icons_drawn_last_frame());
    perf_row(hdc, cursor, tr("Map icons", "Map icons"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "path hit %d / miss %d / %d ms / visible %d",
             sea_lane_render_cache_hits(), sea_lane_render_cache_misses(),
             sea_lane_render_last_ms(), sea_lane_render_visible_routes());
    perf_row(hdc, cursor, tr("Sea lane render", "航道渲染"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s / %s", sea_lane_render_last_reason(),
             sea_lane_render_reason_summary());
    perf_row(hdc, cursor, tr("Sea lane reason", "航道原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "geometry hit %d / miss %d / last %s",
             sea_lane_render_cache_hits(), sea_lane_render_cache_misses(),
             sea_lane_render_last_reason());
    perf_row(hdc, cursor, tr("Route geometry", "Route geometry"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "dash hit %d / miss %d / rebuild %d ms / drawn %d",
             sea_lane_render_dash_cache_hits(), sea_lane_render_dash_cache_misses(),
             sea_lane_render_dash_rebuild_ms(), sea_lane_render_dash_segments());
    perf_row(hdc, cursor, tr("Sea lane dash", "航道虚线"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s / %s", sea_lane_render_dash_reason(),
             sea_lane_render_dash_reason_summary());
    perf_row(hdc, cursor, tr("Dash reason", "虚线原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "infected %d / draw %d ms",
             sea_lane_render_infected_routes(), sea_lane_render_infected_draw_ms());
    perf_row(hdc, cursor, tr("Infected lanes", "感染航道"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "fog %d builds / last %d ms / gate %d ms",
             plague_visual_fog_rebuild_count(), plague_visual_last_fog_rebuild_ms(),
             plague_visual_fog_rebuild_interval_ms());
    perf_row(hdc, cursor, tr("Plague fog", "瘟疫雾"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%dx%d / %s / lanes %d",
             plague_visual_fog_cache_width(), plague_visual_fog_cache_height(),
             plague_visual_mode_text(), plague_visual_infected_lane_count());
    perf_row(hdc, cursor, tr("Plague visual", "瘟疫视觉"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "data %d ms / draw %d ms",
             plague_visual_data_update_ms(), plague_visual_last_draw_ms());
    perf_row(hdc, cursor, tr("Plague split", "瘟疫分层"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s / %s", plague_visual_last_reason(),
             plague_visual_reason_summary());
    perf_row(hdc, cursor, tr("Plague reason", "瘟疫原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Plague system", "瘟疫系统"),
              plague_perf_system_enabled() ? "on" : "off", ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Plague map visuals", "瘟疫地图视觉"),
              plague_perf_map_visuals_enabled() ? "on" : "off", ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Plague sim skipped", "瘟疫模拟跳过"),
              plague_perf_sim_skipped() ? "yes" : "no",
              plague_perf_sim_skipped() ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Plague visual skipped", "瘟疫视觉跳过"),
              plague_perf_visual_skipped() ? "yes" : "no",
              plague_perf_visual_skipped() ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Plague invalidation suppressed", "瘟疫刷新抑制"),
              plague_perf_invalidation_suppressed() ? "yes" : "no",
              plague_perf_invalidation_suppressed() ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "contours %d paths / %d ms",
             perf.contour_path_count, perf.contour_rebuild_ms);
    perf_row(hdc, cursor, tr("Contours", "轮廓线"), text,
              perf.contour_rebuild_ms > 50 ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    draw_worldgen_debug_rows(hdc, cursor);
    { SeaLaneStats sea; sea_lanes_last_stats(&sea);
      snprintf(text, sizeof(text), "lanes %d / ports %d / shallow %d-%d / deep %d / no-admin %d / cap %d", sea.visual_lanes, sea.active_port_nodes, sea.shallow_accepted_edges, sea.shallow_candidate_edges, sea.deep_links, sea.missing_admin_city, sea.max_lane_skips);
      perf_row(hdc, cursor, tr("Sea lanes", "航道网络"), text, ui_theme_color(UI_COLOR_TEXT_MUTED)); }
    {
        int coast_edges;
        int coast_paths;
        int coast_points, coast_loops, coast_bad_loops, coast_kept_loops;
        contour_paths_coast_stats(&coast_edges, &coast_paths, &coast_points,
                                  &coast_loops, &coast_bad_loops, &coast_kept_loops);
        snprintf(text, sizeof(text), "%d edges / %d paths / %d pts / loops %d bad %d kept %d",
                 coast_edges, coast_paths, coast_points, coast_loops, coast_bad_loops, coast_kept_loops);
        perf_row(hdc, cursor, tr("Coast contours", "海岸轮廓"), text,
                  coast_edges > 0 && (coast_paths == 0 || coast_bad_loops > 0) ?
                  RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    }
}
