#include "render/panel_debug_plague.h"

#include "core/plague_perf.h"
#include "core/render_snapshot.h"
#include "render/plague_visual.h"
#include "render/render_context.h"
#include "render_panel_internal.h"
#include "ui/ui_theme.h"

#include <stdio.h>

static void plague_row(HDC hdc, UiCursor *cursor, const char *label,
                       const char *value, COLORREF color) {
    char text[256];
    RECT rect;
    if (cursor->y > cursor->bottom - 20) return;
    snprintf(text, sizeof(text), "%s: %s", label, value);
    rect = (RECT){cursor->x, cursor->y, cursor->x + cursor->width, cursor->y + 20};
    draw_text_rect(hdc, rect, text, color, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    cursor->y += 21;
}

static void draw_model_metrics(HDC hdc, UiCursor *cursor) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    const PlagueMetricsSnapshot *metrics;
    char text[192];
    uint64_t average;
    if (!snapshot) return;
    metrics = &snapshot->plague_metrics;
    average = metrics->step_samples > 0 ? metrics->step_total_us / metrics->step_samples : 0;
    ui_section(hdc, cursor, tr("Plague Model", "瘟疫模型"));
    snprintf(text, sizeof(text), "last %llu us / avg %llu us / peak %llu us / samples %llu",
             (unsigned long long)metrics->step_last_us, (unsigned long long)average,
             (unsigned long long)metrics->step_peak_us,
             (unsigned long long)metrics->step_samples);
    plague_row(hdc, cursor, tr("Step timing", "单步耗时"), text,
               metrics->step_peak_us > 8000 ? RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "episode %d / active %d / due %d / spores %d",
             metrics->episode_id, metrics->active_city_count,
             metrics->due_pulse_count, metrics->spores_remaining);
    plague_row(hdc, cursor, tr("Episode work", "事件工作量"), text,
               ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "edges %d (land %d shallow %d deep %d) / pending %d",
             metrics->candidate_edge_count,
             metrics->candidate_edge_count_by_route[PLAGUE_ROUTE_LAND],
             metrics->candidate_edge_count_by_route[PLAGUE_ROUTE_SHALLOW],
             metrics->candidate_edge_count_by_route[PLAGUE_ROUTE_DEEP],
             metrics->pending_request_count);
    plague_row(hdc, cursor, tr("Spread candidates", "传播候选"), text,
               ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "infect %d / persist %d / dedupe %d / adj #%d rebuild %d (%d us)",
             metrics->committed_infection_count, metrics->committed_persistence_count,
             metrics->deduplicated_request_count, metrics->adjacency_revision,
             metrics->adjacency_rebuild_count, metrics->adjacency_last_rebuild_us);
    plague_row(hdc, cursor, tr("Spread commits", "传播提交"), text,
               ui_theme_color(UI_COLOR_TEXT_MUTED));
}

static void draw_visual_metrics(HDC hdc, UiCursor *cursor) {
    char text[160];
    snprintf(text, sizeof(text), "fog %d builds / last %d ms / gate %d ms",
             plague_visual_fog_rebuild_count(), plague_visual_last_fog_rebuild_ms(),
             plague_visual_fog_rebuild_interval_ms());
    plague_row(hdc, cursor, tr("Plague fog", "瘟疫雾"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%dx%d / %s / lanes %d",
             plague_visual_fog_cache_width(), plague_visual_fog_cache_height(),
             plague_visual_mode_text(), plague_visual_infected_lane_count());
    plague_row(hdc, cursor, tr("Plague visual", "瘟疫视觉"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "data %d ms / draw %d ms / %s / %s",
             plague_visual_data_update_ms(), plague_visual_last_draw_ms(),
             plague_visual_last_reason(), plague_visual_reason_summary());
    plague_row(hdc, cursor, tr("Plague rendering", "瘟疫渲染"), text,
               ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "system %s / visuals %s / sim skip %s / visual skip %s / suppress %s",
             plague_perf_system_enabled() ? "on" : "off",
             plague_perf_map_visuals_enabled() ? "on" : "off",
             plague_perf_sim_skipped() ? "yes" : "no",
             plague_perf_visual_skipped() ? "yes" : "no",
             plague_perf_invalidation_suppressed() ? "yes" : "no");
    plague_row(hdc, cursor, tr("Plague switches", "瘟疫开关"), text,
               plague_perf_sim_skipped() ? RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
}

void draw_debug_plague_rows(HDC hdc, UiCursor *cursor) {
    draw_model_metrics(hdc, cursor);
    draw_visual_metrics(hdc, cursor);
}
