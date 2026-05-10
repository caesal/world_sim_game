#include "render_panel_internal.h"
#include "render/panel_debug.h"

#include "core/profiler.h"
#include "game/game_loop.h"
#include "sim/expansion.h"
#include "ui/ui_widgets.h"

#include <stdio.h>
#include <string.h>

static RECT event_filter_rects[DEBUG_EVENT_FILTER_COUNT];
static int event_filter_rects_valid = 0;

static const char *filter_label(int index) {
    static const char *labels_en[DEBUG_EVENT_FILTER_COUNT] = {"All", "Expansion", "War", "Collapse", "Perf"};
    static const char *labels_zh[DEBUG_EVENT_FILTER_COUNT] = {"全部", "扩张", "战争", "崩溃", "性能"};
    index = clamp(index, 0, DEBUG_EVENT_FILTER_COUNT - 1);
    return tr(labels_en[index], labels_zh[index]);
}

static RECT event_filter_button_rect(RECT client, int index) {
    RECT empty = {0, 0, 0, 0};
    (void)client;
    if (!event_filter_rects_valid || index < 0 || index >= DEBUG_EVENT_FILTER_COUNT) return empty;
    return event_filter_rects[index];
}

int debug_panel_event_filter_hit_test(RECT client, int mouse_x, int mouse_y) {
    int i;
    for (i = 0; i < DEBUG_EVENT_FILTER_COUNT; i++) {
        if (point_in_rect(event_filter_button_rect(client, i), mouse_x, mouse_y)) return i;
    }
    return -1;
}

static int event_matches_filter(int index) {
    EventLogType type = event_log_get_type(index);
    switch (clamp(debug_event_filter, 0, DEBUG_EVENT_FILTER_COUNT - 1)) {
        case DEBUG_EVENT_FILTER_EXPANSION: return type == EVENT_TYPE_EXPANSION_CLAIMED;
        case DEBUG_EVENT_FILTER_WAR: return type == EVENT_TYPE_WAR_STARTED ||
                                             type == EVENT_TYPE_BATTLE_RESOLVED ||
                                             type == EVENT_TYPE_TRUCE_SIGNED;
        case DEBUG_EVENT_FILTER_COLLAPSE: return type == EVENT_TYPE_COLLAPSE_SUCCEEDED ||
                                                  type == EVENT_TYPE_COLLAPSE_FAILED ||
                                                  type == EVENT_TYPE_CIVIL_UNREST_TRIGGERED;
        case DEBUG_EVENT_FILTER_PERFORMANCE: return type == EVENT_TYPE_PERFORMANCE_THROTTLED;
        default: return 1;
    }
}

static COLORREF budget_color(int value, int budget) {
    if (budget <= 0 || value < budget * 7 / 10) return RGB(132, 188, 111);
    if (value <= budget) return RGB(218, 178, 78);
    return RGB(218, 92, 78);
}

static void debug_row(HDC hdc, UiCursor *cursor, const char *label, const char *value, COLORREF color) {
    char text[256];
    RECT rect;
    if (cursor->y > cursor->bottom - 20) return;
    snprintf(text, sizeof(text), "%s: %s", label, value);
    rect = (RECT){cursor->x, cursor->y, cursor->x + cursor->width, cursor->y + 20};
    draw_text_rect(hdc, rect, text, color, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    cursor->y += 21;
}

static void draw_event_filters(HDC hdc, UiCursor *cursor) {
    int i;
    int gap = 5;
    int w = (cursor->width - (DEBUG_EVENT_FILTER_COUNT - 1) * gap) / DEBUG_EVENT_FILTER_COUNT;
    RECT row = ui_take_rect(cursor, 24);
    event_filter_rects_valid = 1;
    for (i = 0; i < DEBUG_EVENT_FILTER_COUNT; i++) {
        RECT button = {cursor->x + i * (w + gap), row.top,
                       i == DEBUG_EVENT_FILTER_COUNT - 1 ? cursor->x + cursor->width :
                       cursor->x + i * (w + gap) + w, row.bottom};
        event_filter_rects[i] = button;
        fill_rect(hdc, button, i == debug_event_filter ? RGB(87, 93, 78) : ui_theme_color(UI_COLOR_PANEL_SOFT));
        draw_center_text(hdc, button, filter_label(i), ui_theme_color(UI_COLOR_TEXT));
    }
    cursor->y += 8;
}

static void draw_phase_timing(HDC hdc, UiCursor *cursor, const RuntimeProfilerSnapshot *perf) {
    char text[160];
    ui_section(hdc, cursor, tr("Phase Timing", "阶段耗时"));
    snprintf(text, sizeof(text), "%s %d ms",
             perf->slowest_phase[0] ? perf->slowest_phase : "none", perf->slowest_phase_ms);
    debug_row(hdc, cursor, tr("Highest cost", "最高耗时"), text,
              budget_color(perf->slowest_phase_ms, perf->sim_budget_ms));
    snprintf(text, sizeof(text), "res %d / pop %d / exp %d / claim %d",
             perf->resource_ms, perf->population_ms, perf->expansion_ms, perf->claim_ms);
    debug_row(hdc, cursor, tr("Core phases", "核心阶段"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "dip %d / plague %d / war-cal %d",
             perf->diplomacy_ms, perf->plague_ms, perf->war_ms);
    debug_row(hdc, cursor, tr("Other phases", "其他阶段"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
}

static void draw_performance_panel(HDC hdc, UiCursor *cursor) {
    RuntimeProfilerSnapshot perf;
    char text[180];

    profiler_snapshot(&perf);
    ui_section(hdc, cursor, tr("Simulation Clock", "模拟时钟"));
    snprintf(text, sizeof(text), "%d / %d ms", perf.frame_avg_ms, perf.frame_peak_ms);
    debug_row(hdc, cursor, tr("Frame avg / peak", "帧均值 / 峰值"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%d ms/month, %.1f months/sec", perf.actual_ms_per_month,
             perf.actual_ms_per_month > 0 ? 1000.0 / perf.actual_ms_per_month : 0.0);
    debug_row(hdc, cursor, tr("Actual month time", "实际每月耗时"), text,
              perf.overloaded ? RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));

    ui_section(hdc, cursor, tr("Queue / Scheduler", "队列 / 调度器"));
    snprintf(text, sizeof(text), "%d ms used of %d ms budget", perf.sim_used_ms, perf.sim_budget_ms);
    debug_row(hdc, cursor, tr("Frame sim budget", "本帧模拟预算"), text,
              budget_color(perf.sim_used_ms, perf.sim_budget_ms));
    snprintf(text, sizeof(text), "pending %d / step %d ms", perf.pending_months, perf.scheduler_step_ms);
    debug_row(hdc, cursor, tr("Queue / step", "队列 / 单步"), text,
              perf.scheduler_step_over_budget ? RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    debug_row(hdc, cursor, tr("Current Job", "当前任务"),
              perf.current_job[0] ? perf.current_job : "Idle", ui_theme_color(UI_COLOR_TEXT_MUTED));
    debug_row(hdc, cursor, tr("Worker", "模拟线程"), game_loop_worker_status(),
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%d ms", game_loop_snapshot_age_ms());
    debug_row(hdc, cursor, tr("Snapshot age", "快照延迟"), text,
              game_loop_snapshot_age_ms() > 500 ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    ui_section(hdc, cursor, tr("Performance", "性能"));
    debug_row(hdc, cursor, tr("Slow Call", "慢调用"),
              perf.last_slow_call[0] ? perf.last_slow_call : tr("None over 50 ms", "无超过 50ms"),
              perf.last_slow_call_ms > perf.sim_budget_ms ? RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    debug_row(hdc, cursor, tr("Status", "状态"),
              perf.overloaded ? tr("Performance limited; backlog prevented", "性能受限，已防止积压") :
                                tr("Responsive", "响应正常"),
              perf.overloaded ? RGB(218, 92, 78) : RGB(132, 188, 111));
    if (perf.overloaded) {
        debug_row(hdc, cursor, tr("Yield", "让出"),
                  tr("This frame yielded to keep UI responsive.", "本帧未继续追赶，优先保持界面响应。"),
                  RGB(218, 178, 78));
    }

    draw_phase_timing(hdc, cursor, &perf);
    ui_section(hdc, cursor, tr("Expansion Counters", "扩张计数"));
    snprintf(text, sizeof(text), "civs %d / regions %d / search %d ms",
             perf.expansion_civs_checked, perf.scanned_regions_this_month,
             perf.expansion_target_search_ms);
    debug_row(hdc, cursor, tr("Search", "搜索"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "attempts %d / success %d / touched %d",
             perf.expansion_claims_attempted, perf.expansion_claims_succeeded,
             perf.claim_tiles_touched);
    debug_row(hdc, cursor, tr("Claims", "占领"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "paths %d / nodes %d",
             perf.maritime_path_searches, perf.maritime_bfs_nodes);
    debug_row(hdc, cursor, tr("Maritime BFS", "海路搜索"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "lanes %d / merged %d / skipped %d / reject %d / %d ms",
             perf.sea_lane_count, perf.sea_lane_merged_routes,
             perf.sea_lane_skipped_routes, perf.route_land_reject_count,
             perf.sea_lane_rebuild_ms);
    debug_row(hdc, cursor, tr("Sea lanes", "视觉航道"), text,
              perf.sea_lane_rebuild_ms > 30 ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));

    ui_section(hdc, cursor, tr("Render Cache", "渲染缓存"));
    snprintf(text, sizeof(text), "%d / %d ms", perf.render_avg_ms, perf.render_peak_ms);
    debug_row(hdc, cursor, tr("Render avg / peak", "渲染均值 / 峰值"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "terrain %d / political %d / borders %d / labels %d / gdi %d",
             perf.terrain_rebuild_count, perf.political_rebuild_count,
             perf.border_rebuild_count, perf.label_rebuild_count,
             perf.gdi_bitmap_recreate_count);
    debug_row(hdc, cursor, tr("Layer rebuilds", "图层重建"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s %dx%d stretch %d",
             perf.terrain_render_mode[0] ? perf.terrain_render_mode : "unknown",
             perf.terrain_cache_width, perf.terrain_cache_height, perf.terrain_stretch_mode);
    debug_row(hdc, cursor, tr("Terrain mode", "地形模式"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "contours %d paths / %d ms",
             perf.contour_path_count, perf.contour_rebuild_ms);
    debug_row(hdc, cursor, tr("Contours", "轮廓线"), text,
              perf.contour_rebuild_ms > 50 ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "regions %d / tiles %d / claimed %d",
             perf.scanned_regions_this_month, perf.scanned_tiles_this_month,
             perf.claimed_regions_this_month);
    debug_row(hdc, cursor, tr("Raw scans", "原始扫描"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
}

static void draw_recent_events(HDC hdc, UiCursor *cursor) {
    int i;
    int shown = 0;

    ui_section(hdc, cursor, tr("Event Log", "事件日志"));
    draw_event_filters(hdc, cursor);
    if (event_log_count <= 0) {
        ui_row_text(hdc, cursor, tr("Events", "事件"), tr("No events yet.", "暂无事件。"));
        return;
    }
    for (i = 0; i < event_log_count && shown < 8; i++) {
        const char *event_text = event_log_get(i);
        if (!event_matches_filter(i)) continue;
        ui_row_text(hdc, cursor, tr("Event", "事件"), event_text);
        shown++;
    }
    if (shown == 0) ui_row_text(hdc, cursor, tr("Events", "事件"), tr("No matching events.", "没有匹配事件。"));
}

void draw_debug_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    UiCursor cursor = ui_cursor(x, TOP_BAR_H + 62, side_panel_w - FORM_X_PAD * 2, client.bottom - 64);
    int i;
    char text[160];

    event_filter_rects_valid = 0;
    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, cursor.y, tr("Debug / Advanced", "调试 / 高级"), ui_theme_color(UI_COLOR_TEXT));
    cursor.y += 32;
    SelectObject(hdc, body_font);
    ui_section(hdc, &cursor, tr("Map Layers", "地图图层"));
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        const char *names_en[MAP_DISPLAY_MODE_COUNT] = {"All", "Climate", "Geography", "Regions", "Political"};
        const char *names_zh[MAP_DISPLAY_MODE_COUNT] = {"全部", "气候", "地理", "区域", "政治"};
        RECT button = get_mode_button_rect(client, i);
        fill_rect(hdc, button, MAP_DISPLAY_MODES[i] == display_mode ? RGB(87, 93, 78) : ui_theme_color(UI_COLOR_PANEL_SOFT));
        draw_center_text(hdc, button, tr(names_en[i], names_zh[i]), ui_theme_color(UI_COLOR_TEXT));
    }
    cursor.y = get_mode_button_rect(client, MAP_DISPLAY_MODE_COUNT - 1).bottom + 10;
    ui_section(hdc, &cursor, tr("Target Speed", "目标速度"));
    snprintf(text, sizeof(text), "%s/month, %.1f months/sec",
             speed_seconds_text(speed_index), 1000.0 / SPEED_MS[clamp(speed_index, 0, SPEED_COUNT - 1)]);
    ui_row_text(hdc, &cursor, tr("Selected", "当前"), text);
    snprintf(text, sizeof(text), "%d ms/month, queue %d",
             game_loop_actual_ms_per_month(), game_loop_pending_months());
    ui_row_text(hdc, &cursor, tr("Measured", "实测"),
                game_loop_actual_ms_per_month() > 0 ? text : tr("No sample yet", "暂无样本"));
    draw_performance_panel(hdc, &cursor);
    draw_recent_events(hdc, &cursor);
    if (selected_civ >= 0 && selected_civ < civ_count) {
        CountrySummary country = summarize_country(selected_civ);
        ui_section(hdc, &cursor, tr("Expansion Gate", "扩张门槛"));
        ui_row_int(hdc, &cursor, tr("Need", "需求"), expansion_need_for_civ(selected_civ, country.resource_score));
        ui_row_int(hdc, &cursor, tr("Threshold", "门槛"), expansion_threshold_for_civ(selected_civ));
        ui_section(hdc, &cursor, tr("Legacy / Internal", "兼容 / 内部"));
        ui_row_int(hdc, &cursor, tr("Legacy culture", "兼容文化"), civs[selected_civ].culture);
    }
}
