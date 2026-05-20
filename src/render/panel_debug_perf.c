#include "render/panel_debug_perf.h"
#include "render/contour_paths.h"
#include "render/panel_debug_worldgen.h"
#include "render/plague_visual.h"
#include "render/render_context.h"
#include "render_panel_internal.h"
#include "core/profiler.h"
#include "game/game_loop.h"
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
    snprintf(text, sizeof(text), "fog %d builds / last %d ms / gate %d ms",
             plague_visual_fog_rebuild_count(), plague_visual_last_fog_rebuild_ms(),
             plague_visual_fog_rebuild_interval_ms());
    perf_row(hdc, cursor, tr("Plague fog", "瘟疫雾"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
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
