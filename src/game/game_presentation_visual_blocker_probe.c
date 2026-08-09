#include "game/game_presentation_visual_blocker_probe.h"

#include "game/game_presentation_decision_artifact_fixture.h"
#include "game/game_presentation_static_physical_artifacts.h"
#include "render/panel_country_detail.h"
#include "render/render_context.h"
#include "render/render_panel_internal.h"
#include "sim/decision_snapshot_cache.h"
#include "ui/ui_theme.h"
#include "ui/ui_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VISUAL_BLOCKER_ARTIFACT_HEIGHT 1369
#define VISUAL_BLOCKER_MAP_STRIP 80

typedef struct {
    const char *name;
    const char *reason;
    const char *expected_zh;
    int next_months;
} OverviewReasonCase;

typedef struct {
    int selected_civ;
    int ui_language;
    int side_panel_w;
    int side_panel_collapsed;
    int panel_tab;
    int country_detail_subtab;
    int country_decision_subtab;
    int display_mode;
    int selected_alliance_id;
    int world_generated;
    int country_list_scroll_offset;
    int country_detail_scroll_offset;
    int country_detail_scroll_offsets[8];
    int hover_x;
    int hover_y;
} VisualBlockerUiState;

static const OverviewReasonCase production_cases[] = {
    {"no_decision", "No expansion decision yet.", "尚无扩张决策", 0},
    {"no_adjacent", "No adjacent unowned natural region found; land blocked or unreachable.",
     "无相邻可占领自然区域；陆路受阻或不可达", 0},
    {"frontier_drive", "Frontier found, but population/resource/admin drive 37 <= 84.",
     "已发现边疆；人口/资源/行政驱动力 37 <= 84", 0},
    {"claim_city_cap", "Region claim failed: city cap or no admin city available.",
     "区域占领失败：城市上限或无可用行政城市", 0},
    {"claim_ownership", "Region claim failed: ownership or admin-city constraints blocked it.",
     "区域占领失败：所有权或行政城市条件阻止占领", 0},
    {"claimed_adjacent", "Claimed adjacent region 314; cooldown 17 months; tech expansion x82%.",
     "已占领相邻区域 314；冷却 17 个月；科技扩张 x82%", 17},
    {"cooldown", "Expansion cooldown: next claim window in 9 months.",
     "扩张冷却：距下次占领窗口 9 个月", 9},
    {"skipped_desire", "Claim skipped: desire 61 vs threshold 73; land 4, shallow 2, maritime 5.",
     "跳过占领：意愿 61/阈值 73；陆地 4，浅海 2，航路 5", 0},
    {"stability_block", "Stability gate blocks overseas expansion.",
     "稳定闸门阻止海外扩张", 0},
    {"claimed_sea", "Claimed sea-reachable region; shallow 3, maritime 7, deep 2; cooldown 11 months.",
     "已通过航道占领新区域；浅海 3，航路 7，深海 2；冷却 11 个月", 11},
    {"sea_skip", "Sea reachable S3/R7/D2; skipped by chance, score, or city cap.",
     "海路可达：浅海 3/航路 7/深海 2；机会、评分或城市上限未通过", 0},
    {"no_land_or_sea", "Land: no adjacent target. Shallow sea: 0 reachable. Maritime: 0 reachable. Port candidates: 6.",
     "无陆地、浅海或航路目标；港口候选 6", 0},
    {"nearby_land", "Nearby land remains; shallow islands are visible but homeland expansion is preferred.",
     "附近仍有陆地；优先扩张本土而非浅海岛屿", 0}
};

static const OverviewReasonCase legacy_cases[] = {
    {"claimed_adjacent_legacy", "Claimed adjacent region", "已占领相邻区域，进入冷却", 0},
    {"claimed_nearby_legacy", "Claimed nearby region", "已占领附近区域", 0},
    {"claimed_shallow_legacy", "Claimed shallow target", "已占领浅海可达区域，进入冷却", 0},
    {"claimed_maritime_legacy", "Claimed maritime target", "已通过航道占领新区域", 0},
    {"claimed_overseas_legacy", "Claimed overseas target", "已通过航道占领新区域", 0}
};

static VisualBlockerUiState save_ui_state(void) {
    VisualBlockerUiState saved;
    saved.selected_civ = selected_civ;
    saved.ui_language = ui_language;
    saved.side_panel_w = side_panel_w;
    saved.side_panel_collapsed = side_panel_collapsed;
    saved.panel_tab = panel_tab;
    saved.country_detail_subtab = country_detail_subtab;
    saved.country_decision_subtab = country_decision_subtab;
    saved.display_mode = display_mode;
    saved.selected_alliance_id = selected_alliance_id;
    saved.world_generated = world_generated;
    saved.country_list_scroll_offset = country_list_scroll_offset;
    saved.country_detail_scroll_offset = country_detail_scroll_offset;
    memcpy(saved.country_detail_scroll_offsets, country_detail_scroll_offsets,
           sizeof(saved.country_detail_scroll_offsets));
    saved.hover_x = hover_x;
    saved.hover_y = hover_y;
    return saved;
}

static void restore_ui_state(const VisualBlockerUiState *saved) {
    selected_civ = saved->selected_civ;
    ui_language = saved->ui_language;
    side_panel_w = saved->side_panel_w;
    side_panel_collapsed = saved->side_panel_collapsed;
    panel_tab = saved->panel_tab;
    country_detail_subtab = saved->country_detail_subtab;
    country_decision_subtab = saved->country_decision_subtab;
    display_mode = saved->display_mode;
    selected_alliance_id = saved->selected_alliance_id;
    world_generated = saved->world_generated;
    country_list_scroll_offset = saved->country_list_scroll_offset;
    country_detail_scroll_offset = saved->country_detail_scroll_offset;
    memcpy(country_detail_scroll_offsets, saved->country_detail_scroll_offsets,
           sizeof(saved->country_detail_scroll_offsets));
    hover_x = saved->hover_x;
    hover_y = saved->hover_y;
}

static int reason_case_ok(const OverviewReasonCase *test, FILE *summary) {
    DecisionSnapshot decision;
    const char *actual;
    int old_language = ui_language;
    int en_ok, zh_ok;
    memset(&decision, 0, sizeof(decision));
    decision.expansion_reason = test->reason;
    decision.main_intent = "Waiting";
    decision.next_expansion_months = test->next_months;
    ui_language = UI_LANG_ZH;
    actual = country_overview_next_action_text(&decision, UI_LANG_EN);
    en_ok = actual && strcmp(actual, test->reason) == 0;
    ui_language = UI_LANG_EN;
    actual = country_overview_next_action_text(&decision, UI_LANG_ZH);
    zh_ok = actual && strcmp(actual, test->expected_zh) == 0;
    ui_language = old_language;
    fprintf(summary, "case=country_overview_reason_%s ok=%d en=%d zh=%d\n",
            test->name, en_ok && zh_ok, en_ok, zh_ok);
    return en_ok && zh_ok;
}

static int intent_cases_ok(FILE *summary) {
    static const struct {
        const char *intent;
        const char *expected_zh;
    } cases[] = {
        {"Waiting", "等待"}, {"Expansion", "倾向扩张"},
        {"War", "战争倾向上升"}, {"Stability", "优先维持稳定"}
    };
    DecisionSnapshot decision;
    int i, passed = 0;
    memset(&decision, 0, sizeof(decision));
    decision.expansion_reason = "";
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        const char *actual;
        decision.main_intent = cases[i].intent;
        actual = country_overview_next_action_text(&decision, UI_LANG_ZH);
        if (actual && strcmp(actual, cases[i].expected_zh) == 0) passed++;
    }
    decision.expansion_reason = "Unrecognized future expansion reason.";
    decision.main_intent = "Expansion";
    fprintf(summary,
            "case=country_overview_reason_intents ok=%d passed=%d/%d fallback=%d\n",
            passed == (int)(sizeof(cases) / sizeof(cases[0])) &&
            strcmp(country_overview_next_action_text(&decision, UI_LANG_ZH),
                   "扩张决策详情暂不可用") == 0,
            passed, (int)(sizeof(cases) / sizeof(cases[0])),
            strcmp(country_overview_next_action_text(&decision, UI_LANG_ZH),
                   "扩张决策详情暂不可用") == 0);
    return passed == (int)(sizeof(cases) / sizeof(cases[0])) &&
           strcmp(country_overview_next_action_text(&decision, UI_LANG_ZH),
                  "扩张决策详情暂不可用") == 0;
}

static void set_artifact_ui(int language, int panel_width, int detail_tab) {
    int i;
    selected_civ = 0;
    ui_language = language;
    side_panel_w = panel_width;
    side_panel_collapsed = 0;
    panel_tab = PANEL_COUNTRY;
    country_detail_subtab = detail_tab;
    country_decision_subtab = COUNTRY_DECISION_WAR;
    display_mode = DISPLAY_POLITICAL;
    selected_alliance_id = -1;
    world_generated = 1;
    country_list_scroll_offset = 0;
    country_detail_scroll_offset = 0;
    for (i = 0; i < 8; i++) country_detail_scroll_offsets[i] = 0;
    hover_x = -1;
    hover_y = -1;
}

static int render_shell_artifact(RenderSnapshot *snapshot,
                                 const char *artifact_dir, FILE *summary,
                                 int language, int panel_width,
                                 int detail_tab) {
    StaticPhysicalProbeCanvas canvas;
    const RenderSnapshot *previous_context = render_context_snapshot();
    int client_width = panel_width + VISUAL_BLOCKER_MAP_STRIP;
    const char *view = detail_tab == COUNTRY_DETAIL_OVERVIEW ?
                       "country_overview" : "decision_war";
    const char *lang = language == UI_LANG_ZH ? "zh" : "en";
    char name[128];
    uint64_t hash;
    int wrote;
    if (!static_physical_probe_canvas_open(
            &canvas, client_width, VISUAL_BLOCKER_ARTIFACT_HEIGHT)) return 0;
    set_artifact_ui(language, panel_width, detail_tab);
    fill_rect(canvas.dc,
              (RECT){0, 0, client_width, VISUAL_BLOCKER_ARTIFACT_HEIGHT},
              RGB(35, 52, 58));
    render_context_begin(snapshot);
    draw_side_panel(canvas.dc,
                    (RECT){0, 0, client_width, VISUAL_BLOCKER_ARTIFACT_HEIGHT});
    if (previous_context) render_context_begin(previous_context);
    else render_context_end();
    GdiFlush();
    hash = static_physical_probe_canvas_hash(&canvas);
    snprintf(name, sizeof(name), "visual_blocker_%s_%s_%d.bmp",
             view, lang, panel_width);
    wrote = static_physical_probe_canvas_write(&canvas, artifact_dir, name);
    fprintf(summary,
            "case=visual_blocker_artifact view=%s language=%s width=%d ok=%d hash=%llu file=%s\n",
            view, lang, panel_width, wrote && hash != 0,
            (unsigned long long)hash, name);
    static_physical_probe_canvas_close(&canvas);
    return wrote && hash != 0;
}

int game_presentation_visual_blocker_probe(
    const char *artifact_dir, FILE *summary) {
    VisualBlockerUiState saved = save_ui_state();
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    uint64_t calculations_before =
        decision_snapshot_cache_total_calculation_count();
    int production_passed = 0;
    int legacy_passed = 0;
    int artifacts = 0;
    int i, language, width_index;
    const int widths[] = {500, 720};
    int ok;
    if (!snapshot) return 0;
    for (i = 0; i < (int)(sizeof(production_cases) /
                          sizeof(production_cases[0])); i++) {
        if (reason_case_ok(&production_cases[i], summary)) production_passed++;
    }
    for (i = 0; i < (int)(sizeof(legacy_cases) / sizeof(legacy_cases[0])); i++) {
        if (reason_case_ok(&legacy_cases[i], summary)) legacy_passed++;
    }
    ok = production_passed == (int)(sizeof(production_cases) /
                                    sizeof(production_cases[0])) &&
         legacy_passed == (int)(sizeof(legacy_cases) / sizeof(legacy_cases[0])) &&
         intent_cases_ok(summary);
    game_presentation_decision_artifact_fill_fixture(snapshot);
    snprintf(snapshot->civs[0].decision_expansion_reason,
             sizeof(snapshot->civs[0].decision_expansion_reason), "%s",
             production_cases[1].reason);
    snapshot->civs[0].decision.expansion_reason =
        snapshot->civs[0].decision_expansion_reason;
    for (language = UI_LANG_EN; language <= UI_LANG_ZH; language++) {
        for (width_index = 0; width_index < 2; width_index++) {
            artifacts += render_shell_artifact(
                snapshot, artifact_dir, summary, language,
                widths[width_index], COUNTRY_DETAIL_OVERVIEW);
            artifacts += render_shell_artifact(
                snapshot, artifact_dir, summary, language,
                widths[width_index], COUNTRY_DETAIL_DECISION);
        }
    }
    restore_ui_state(&saved);
    free(snapshot);
    ok &= artifacts == 8 &&
          decision_snapshot_cache_total_calculation_count() ==
          calculations_before;
    fprintf(summary,
            "case=visual_blocker_overall ok=%d production=%d/%d legacy=%d/%d artifacts=%d/8 calculations=%llu/%llu\n",
            ok, production_passed,
            (int)(sizeof(production_cases) / sizeof(production_cases[0])),
            legacy_passed, (int)(sizeof(legacy_cases) / sizeof(legacy_cases[0])),
            artifacts, (unsigned long long)calculations_before,
            (unsigned long long)decision_snapshot_cache_total_calculation_count());
    return ok;
}
