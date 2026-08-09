#include "render/panel_debug_perf.h"
#include "render/contour_paths.h"
#include "render/diplomacy_map_anim.h"
#include "render/panel_debug_spikes.h"
#include "render/panel_debug_plague.h"
#include "render/panel_debug_worldgen.h"
#include "render/panel_view_model_cache.h"
#include "render/map_highlight.h"
#include "render/map_labels.h"
#include "render/map_label_placement_pool.h"
#include "render/render.h"
#include "render/render_static_map_cache.h"
#include "render/sea_lane_render.h"
#include "render/render_context.h"
#include "render_panel_internal.h"
#include "core/dirty_flags.h"
#include "core/profiler.h"
#include "core/render_snapshot_cache.h"
#include "core/render_snapshot_profile.h"
#include "game/game_loop.h"
#include "sim/alliance.h"
#include "sim/civ_colors.h"
#include "sim/diplomacy_year.h"
#include "sim/fragmentation_diag.h"
#include "sim/sea_lanes.h"
#include "ui/ui_theme.h"
#include <stdio.h>
#include <string.h>

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

static void draw_economy_debug_rows(HDC hdc, UiCursor *cursor) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int i, alive = 0, pressure_sum = 0, pressure_max = 0, projects = 0, merc_cooldowns = 0;
    long long treasury = 0, cap = 0, pending = 0;
    char text[180];
    if (!snapshot) return;
    for (i = 0; i < snapshot->civ_count; i++) {
        const SnapshotCiv *civ = &snapshot->civs[i];
        if (!civ->alive) continue;
        alive++;
        treasury += civ->treasury;
        cap += civ->treasury_cap;
        pending += civ->treasury_pending_surplus;
        pressure_sum += civ->resource_pressure;
        pressure_max = max(pressure_max, civ->resource_pressure);
        if (civ->treasury_stability_months_left > 0) projects++;
        if (civ->mercenary_cooldown_months > 0) merc_cooldowns++;
    }
    snprintf(text, sizeof(text), "treasury %lld / cap %lld / pending %lld", treasury, cap, pending);
    perf_row(hdc, cursor, tr("Treasury world", "世界国库"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "pressure avg %d / max %d / stability %d / merc cd %d",
             alive > 0 ? pressure_sum / alive : 0, pressure_max, projects, merc_cooldowns);
    perf_row(hdc, cursor, tr("Resource economy", "资源经济"), text,
             pressure_max >= 85 ? RGB(218, 92, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
}

static void draw_rule39_evidence_rows(HDC hdc, UiCursor *cursor) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    char text[180];
    char list[180] = "";
    int stage5 = 0;
    int stage6 = 0;

    if (snapshot) {
        int i;
        for (i = 0; i < snapshot->civ_count && i < MAX_CIVS; i++) {
            const SnapshotCiv *civ = &snapshot->civs[i];
            if (!civ->alive) continue;
            if (civ->tech_stage >= 6) stage6++;
            if (civ->tech_stage >= 5) {
                if (stage5 < 5) {
                    char item[64];
                    snprintf(item, sizeof(item), "%s%d:%s/s%d",
                             stage5 > 0 ? " | " : "", civ->id, civ->name_en, civ->tech_stage);
                    strncat(list, item, sizeof(list) - strlen(list) - 1);
                }
                stage5++;
            }
        }
    }
    perf_row(hdc, cursor, tr("Rule39 stage5 civs", "Rule39 stage5 civs"),
             list[0] ? list : "none", stage5 >= 5 ? RGB(132, 188, 111) : ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "stage5 %d / stage6 %d / routes %d shallow %d deep %d",
             stage5, stage6, sea_lane_render_visible_routes(),
             sea_lane_render_visible_shallow_routes(), sea_lane_render_visible_deep_routes());
    perf_row(hdc, cursor, tr("Rule39 routes", "Rule39 routes"),
             text, sea_lane_render_visible_deep_routes() > 0 ? RGB(132, 188, 111) : ui_theme_color(UI_COLOR_TEXT_MUTED));
}

void draw_debug_performance_panel(HDC hdc, UiCursor *cursor) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    RuntimeProfilerSnapshot perf;
    char text[180];
    profiler_snapshot(&perf);
    draw_debug_spike_rows(hdc, cursor);
    draw_rule39_evidence_rows(hdc, cursor);
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
             snapshot ? snapshot->decision_cache_valid_count : 0,
             snapshot ? snapshot->decision_cache_dirty_count : 0,
             snapshot ? snapshot->decision_cache_last_update_count : 0,
             snapshot ? snapshot->decision_cache_last_update_ms : 0);
    perf_row(hdc, cursor, tr("Decision cache", "决策缓存"), text,
              snapshot && snapshot->decision_cache_dirty_count > 0 ? RGB(218, 178, 78) :
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "cached %d / stale %d / fallback %d",
             snapshot ? snapshot->decision_snapshot_cached_count : 0,
             snapshot ? snapshot->decision_snapshot_stale_count : 0,
             snapshot ? snapshot->decision_snapshot_fallback_count : 0);
    perf_row(hdc, cursor, tr("Snapshot decisions", "快照决策"), text,
              snapshot && snapshot->decision_snapshot_fallback_count > 0 ? RGB(218, 178, 78) :
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    ui_section(hdc, cursor, tr("Simulation Clock", "模拟时钟"));
    if (render_context_snapshot()) {
        const FragmentationDiagnostics *frag = &render_context_snapshot()->fragmentation;
        snprintf(text, sizeof(text), "alive %d / ind %d / vas %d / one %d / slots %d/%d",
                 frag->alive_civs, frag->independent_civs, frag->vassal_civs,
                 frag->one_province_civs, frag->used_slots, frag->max_civs);
        perf_row(hdc, cursor, tr("Fragmentation civs", "碎裂国家"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
        snprintf(text, sizeof(text), "I %d / V %d / J %d / U %d / fb slot %d no-land %d",
                 frag->enclave_independent, frag->enclave_original_vassal,
                 frag->enclave_joined_land_neighbor, frag->enclave_unowned_collapse,
                 frag->enclave_fallback_slot_full, frag->enclave_fallback_no_land_neighbor);
        perf_row(hdc, cursor, tr("Enclave outcomes", "飞地结果"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
        snprintf(text, sizeof(text), "try %d / ok %d / fail %d / succ %d / claimfail %d",
                 frag->collapse_attempts, frag->collapse_successes, frag->collapse_failures,
                 frag->collapse_successors_created, frag->enclave_claim_failures);
        perf_row(hdc, cursor, tr("Collapse diag", "崩溃诊断"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
        snprintf(text, sizeof(text), "release %d / peace %d / war %d",
                 frag->vassal_releases, frag->vassal_peaceful_independence,
                 frag->vassal_independence_wars);
        perf_row(hdc, cursor, tr("Vassal breaks", "附庸脱离"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    }
    draw_economy_debug_rows(hdc, cursor);
    perf_row(hdc, cursor, tr("Snapshot city cache", "快照城市缓存"),
              render_snapshot_cache_city_summary_debug(), ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Snapshot diplomacy cache", "快照外交缓存"),
              render_snapshot_cache_diplomacy_debug(), ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Snapshot plague cache", "快照瘟疫缓存"),
              render_snapshot_cache_plague_debug(), ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Lane snapshot source", "航线快照来源"),
              render_snapshot_cache_lane_debug(), ui_theme_color(UI_COLOR_TEXT_MUTED));
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
    snprintf(text, sizeof(text), "pending %d / max %d / shown %d / dropped %d / skipped %d / throttle %s",
             game_loop_presentation_backlog(), game_loop_visual_max_backlog(),
             game_loop_visual_presented_total(), game_loop_visual_dropped_months(),
             game_loop_displayed_month_order_skips(),
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
    snprintf(text, sizeof(text), "changed %d / unresolved %d / cooldown %d",
             civilization_color_repair_changed_count(),
             civilization_color_repair_unresolved_count(),
             civilization_color_repair_cooldown_count());
    perf_row(hdc, cursor, tr("Color repair", "Color repair"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "dip %d/%d ms / alliance %d/%d ms",
             diplomacy_year_last_step_ms(), diplomacy_year_peak_step_ms(),
             alliance_year_last_step_ms(), alliance_year_peak_step_ms());
    perf_row(hdc, cursor, tr("Annual steps", "Annual steps"), text,
              ui_theme_color(UI_COLOR_TEXT_MUTED));
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
    snprintf(text, sizeof(text), "%s #%d / cur %d / pres %d / safe %d full %d ns %d stale %d / %s",
             render_scene_cache_last_reason(), render_scene_cache_last_reason_code(),
             render_static_scene_presented_current(), render_static_scene_presentable(),
             render_static_scene_complete(), render_static_scene_fully_current(),
             render_static_scene_no_safe_frames(), render_static_scene_stale_safe_age_ms(),
             render_scene_cache_reason_summary());
    perf_row(hdc, cursor, tr("Scene reason", "场景原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s / safe %d full %d / %s", render_static_map_cache_last_reason(),
             render_static_map_cache_presented_boundary_safe(),
             render_static_map_cache_presented_fully_current(),
             render_static_map_cache_reason_summary());
    perf_row(hdc, cursor, tr("Static reason", "静态原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "source %s / active %d / delayed %s / block %d",
             diplomacy_map_anim_source(), diplomacy_map_anim_active_count(),
             diplomacy_map_anim_delayed_waiting_for_snapshot() ? "yes" : "no",
             diplomacy_map_anim_cached_paint_blocked_count());
    perf_row(hdc, cursor, tr("Diplomacy anim", "外交动画"), text,
              diplomacy_map_anim_delayed_waiting_for_snapshot() ? RGB(218, 178, 78) :
              ui_theme_color(UI_COLOR_TEXT_MUTED));
    if (render_context_snapshot()) {
        snprintf(text, sizeof(text), "consumed %d / snap %d / enq %d draw %d exp %d",
                 diplomacy_map_anim_last_consumed_total(),
                 render_context_snapshot()->event_total_entries,
                 diplomacy_map_anim_enqueued_count(), diplomacy_map_anim_drawn_count(),
                 diplomacy_map_anim_expired_before_draw_count());
        perf_row(hdc, cursor, tr("Diplomacy events", "外交事件"), text,
                  ui_theme_color(UI_COLOR_TEXT_MUTED));
        snprintf(text, sizeof(text), "tiles %d / static %d / live %d",
                 render_context_snapshot()->tiles_revision,
                 render_static_map_cache_snapshot_revision(),
                 render_static_map_cache_live_revision());
        perf_row(hdc, cursor, tr("Map revisions", "地图修订"), text,
                  render_static_map_cache_snapshot_revision() != render_static_map_cache_live_revision() ?
                  RGB(218, 178, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
        snprintf(text, sizeof(text), "snap f/b %d/%d / live f/b %d/%d / pub f/b %d/%d / own cur %d",
                 render_static_map_cache_snapshot_fill_revision(),
                 render_static_map_cache_snapshot_border_revision(),
                 render_static_map_cache_live_fill_revision(),
                 render_static_map_cache_live_border_revision(),
                 render_static_map_cache_published_fill_revision(),
                 render_static_map_cache_published_border_revision(),
                 render_static_map_cache_ownership_current());
        perf_row(hdc, cursor, tr("Political keys", "政治键"), text,
                 render_static_map_cache_ownership_current() ?
                 ui_theme_color(UI_COLOR_TEXT_MUTED) : RGB(218, 178, 78));
        snprintf(text, sizeof(text), "pending %d ms / last visible %d ms",
                 render_static_map_cache_political_pending_ms(),
                 render_static_map_cache_political_last_latency_ms());
        perf_row(hdc, cursor, tr("Political latency", "政治延迟"), text,
                 render_static_map_cache_political_pending_ms() > 1000 ?
                 RGB(218, 118, 78) : ui_theme_color(UI_COLOR_TEXT_MUTED));
        snprintf(text, sizeof(text), "data %d / visual %d / pop %d",
                 render_context_snapshot()->cities_revision,
                 render_context_snapshot()->city_visual_revision,
                 dirty_revision_population());
        perf_row(hdc, cursor, tr("City revisions", "City revisions"), text,
                  ui_theme_color(UI_COLOR_TEXT_MUTED));
    }
    snprintf(text, sizeof(text), "%s / build %d ms / age %d ms / refresh %d",
             panel_view_model_cache_key_type(), panel_view_model_cache_last_build_ms(),
             panel_view_model_cache_age_ms(), panel_view_model_cache_refresh_count());
    perf_row(hdc, cursor, tr("Side panel cache", "侧栏缓存"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "%s / %s", panel_view_model_cache_last_reason(),
             panel_view_model_cache_reason_summary());
    perf_row(hdc, cursor, tr("Panel reason", "侧栏原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "last %s / full %d / hover %d / throttle %d",
             panel_view_model_cache_last_invalidation(),
             panel_view_model_cache_full_invalidation_count(),
             panel_view_model_cache_hover_invalidation_count(),
             panel_view_model_cache_throttle_count());
    perf_row(hdc, cursor, tr("Panel invalidation", "Panel invalidation"), text,
             ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "draw %d/%d / source %d / place %d / %d ms",
             map_label_cache_drawn_count(), map_label_cache_candidate_count(),
             map_label_cache_source_rebuild_count(), map_label_cache_placement_rebuild_count(),
             map_label_cache_last_rebuild_ms());
    perf_row(hdc, cursor, tr("Label layout", "标签布局"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "source %s / placement %s / preview %d",
             map_label_cache_source_last_reason(), map_label_cache_placement_last_reason(),
             map_label_cache_preview_skip_count());
    perf_row(hdc, cursor, tr("Label reason", "标签原因"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Label cache", "Label cache"), map_label_cache_reason_summary(),
             ui_theme_color(UI_COLOR_TEXT_MUTED));
    {
        const MapLabelPlacementPoolStats *pool = map_label_placement_pool_stats();
        snprintf(text, sizeof(text), "hit %d / miss %d / store %d / evict %d / ready %d / %llu B",
                 pool->hits, pool->misses, pool->stores, pool->evictions,
                 pool->ready_entries, (unsigned long long)pool->persistent_bytes);
        perf_row(hdc, cursor, tr("Label pool", "标签池"), text,
                 ui_theme_color(UI_COLOR_TEXT_MUTED));
    }
    snprintf(text, sizeof(text), "paints %d / req %d / clear %d/%d / ms %d peak %d",
             map_highlight_overlay_call_count(), map_highlight_overlay_last_requests(),
             map_highlight_overlay_last_clears(), map_highlight_overlay_total_clears(),
             map_highlight_overlay_last_ms(), map_highlight_overlay_peak_ms());
    perf_row(hdc, cursor, tr("Highlight overlay", "高亮叠层"), text,
             ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "surface recreate %d / reuse %d / tiles %d / rects %d",
             map_highlight_overlay_recreate_count(), map_highlight_overlay_reuse_count(),
             map_highlight_overlay_last_tiles(), map_highlight_overlay_last_rects());
    perf_row(hdc, cursor, tr("Highlight surface", "高亮表面"), text,
             ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "calls %d / req %d / hit %d miss %d / tiles %d / seg %d / pens %d / ms %d peak %d",
             map_highlight_edge_call_count(), map_highlight_edge_last_requests(),
             map_highlight_edge_cache_hits(), map_highlight_edge_cache_misses(),
             map_highlight_edge_last_tiles(), map_highlight_edge_last_segments(),
             map_highlight_edge_last_pen_creates(), map_highlight_edge_last_ms(),
             map_highlight_edge_peak_ms());
    perf_row(hdc, cursor, tr("Highlight edge", "高亮边界"), text,
             ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "geom %d peak %d / layer %d peak %d / blit %d peak %d / hit-only %d",
             map_highlight_edge_geometry_last_ms(), map_highlight_edge_geometry_peak_ms(),
             map_highlight_edge_layer_last_rebuild_ms(), map_highlight_edge_layer_peak_rebuild_ms(),
             map_highlight_edge_layer_last_blit_ms(), map_highlight_edge_layer_peak_blit_ms(),
             map_highlight_edge_layer_last_hit_blit_ms());
    perf_row(hdc, cursor, tr("Highlight edge detail", "高亮边界详情"), text,
             ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "calls %d / req %d / rings %d / pens %d / fallback tiles %d / ms %d peak %d",
             map_highlight_focus_call_count(), map_highlight_focus_last_requests(),
             map_highlight_focus_last_rings(), map_highlight_focus_last_pen_creates(),
             map_highlight_focus_last_fallback_tiles(), map_highlight_focus_last_ms(),
             map_highlight_focus_peak_ms());
    perf_row(hdc, cursor, tr("Highlight focus", "高亮焦点"), text,
             ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "paints %d / ms %d peak %d",
             map_highlight_total_paint_count(), map_highlight_total_last_ms(),
             map_highlight_total_peak_ms());
    perf_row(hdc, cursor, tr("Highlight total", "高亮总计"), text,
             ui_theme_color(UI_COLOR_TEXT_MUTED));
    perf_row(hdc, cursor, tr("Label revisions", "Label revisions"),
              dirty_label_revision_summary(), ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "cities %d / neutral %d / ports %d",
             render_city_icons_drawn_last_frame(), render_neutral_city_icons_drawn_last_frame(),
             render_port_icons_drawn_last_frame());
    perf_row(hdc, cursor, tr("Map icons", "Map icons"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "hit %d / miss %d / rebuild %d / preview %d / rb %d/%d blit %d/%d / %s",
             render_city_overlay_cache_hits(), render_city_overlay_cache_misses(),
             render_city_overlay_exact_rebuilds(), render_city_overlay_preview_reuses(),
             render_city_overlay_last_rebuild_ms(), render_city_overlay_peak_rebuild_ms(),
             render_city_overlay_last_blit_ms(), render_city_overlay_peak_blit_ms(),
             render_overlay_cache_last_reason());
    perf_row(hdc, cursor, tr("City overlay", "City overlay"), text, ui_theme_color(UI_COLOR_TEXT_MUTED));
    snprintf(text, sizeof(text), "path hit %d / miss %d / %d ms / visible %d s:%d d:%d",
             sea_lane_render_cache_hits(), sea_lane_render_cache_misses(),
             sea_lane_render_last_ms(), sea_lane_render_visible_routes(),
             sea_lane_render_visible_shallow_routes(), sea_lane_render_visible_deep_routes());
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
    draw_debug_plague_rows(hdc, cursor);
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
