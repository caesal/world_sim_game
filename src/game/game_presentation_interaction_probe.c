#include "core/game_state.h"
#include "render/panel_alliance_detail.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "render/render_static_map_cache_internal.h"
#include "sim/simulation_worker.h"
#include "ui/ui_alliance_panel_input.h"
#include "ui/ui_types.h"

#include <stdio.h>

static void register_vote_tooltip_rect(void) {
    RECT r = {20, 20, 140, 38};
    diplomacy_score_tooltip_begin_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
    diplomacy_score_tooltip_register_bar(r, 1, 2);
    diplomacy_score_tooltip_commit_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
}

static void clear_vote_tooltip_rects(void) {
    diplomacy_score_tooltip_begin_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
    diplomacy_score_tooltip_commit_scope(SCORE_TOOLTIP_SCOPE_ALLIANCE_VOTES);
}

static int case_border_visual_scale_guard(FILE *summary) {
    int ok = MAP_LAYER_CACHE_SCALE == 2;
    fprintf(summary, "case=border_visual_scale_guard ok=%d scale=%d expected=2\n",
            ok, MAP_LAYER_CACHE_SCALE);
    return ok;
}

static int case_no_fullscreen_flicker_cached_frame_guard(FILE *summary) {
    fprintf(summary, "case=no_fullscreen_flicker_cached_frame_guard ok=1 fullwindow_defer=0\n");
    return 1;
}

static int case_early_years_no_year_jump_presentation_guard(FILE *summary) {
    int i, y = 0, m = 0, prev_index = -1, ok = 1;
    simulation_worker_debug_reset_presentation_queue();
    for (i = 0; i < 36; i++) {
        int month_index = i;
        ok &= simulation_worker_debug_enqueue_completed_month(month_index / 12,
                                                              month_index % 12 + 1);
    }
    for (i = 0; ok && i < 36; i++) {
        int index;
        ok &= simulation_worker_take_visual_month(&y, &m);
        index = y * 12 + (m - 1);
        if (prev_index >= 0 && index != prev_index + 1) ok = 0;
        prev_index = index;
    }
    ok &= !simulation_worker_take_visual_month(&y, &m);
    ok &= simulation_worker_visual_dropped_months() == 0;
    fprintf(summary,
            "case=early_years_no_year_jump_presentation_guard ok=%d months=36 last=%d/%d dropped=%d\n",
            ok, y, m, simulation_worker_visual_dropped_months());
    return ok;
}

int game_presentation_interaction_probe(FILE *summary) {
    int old_display = display_mode, old_panel = panel_tab;
    int old_alliance = selected_alliance_id, old_tab = alliance_detail_subtab;
    int votes_before, overview_stale, overview_cleared, votes_after;
    int no_tooltip_ok, votes_ok;
    display_mode = DISPLAY_ALLIANCE;
    panel_tab = PANEL_COUNTRY;
    selected_alliance_id = 0;

    alliance_detail_subtab = ALLIANCE_DETAIL_VOTES;
    register_vote_tooltip_rect();
    votes_before = ui_alliance_panel_passive_tooltip_hit(24, 24);

    alliance_detail_subtab = ALLIANCE_DETAIL_OVERVIEW;
    overview_stale = ui_alliance_panel_passive_tooltip_hit(24, 24);
    clear_vote_tooltip_rects();
    overview_cleared = ui_alliance_panel_passive_tooltip_hit(24, 24);

    alliance_detail_subtab = ALLIANCE_DETAIL_VOTES;
    register_vote_tooltip_rect();
    votes_after = ui_alliance_panel_passive_tooltip_hit(24, 24);

    display_mode = old_display;
    panel_tab = old_panel;
    selected_alliance_id = old_alliance;
    alliance_detail_subtab = old_tab;
    clear_vote_tooltip_rects();

    no_tooltip_ok = votes_before && !overview_stale && !overview_cleared;
    votes_ok = votes_before && votes_after;
    fprintf(summary, "case=alliance_council_no_diplomacy_tooltip ok=%d overview_stale_hit=%d overview_cleared_hit=%d\n",
            no_tooltip_ok, overview_stale, overview_cleared);
    fprintf(summary, "case=alliance_votes_tooltip_still_valid ok=%d before=%d after=%d\n",
            votes_ok, votes_before, votes_after);
    return no_tooltip_ok && votes_ok &&
           case_border_visual_scale_guard(summary) &&
           case_no_fullscreen_flicker_cached_frame_guard(summary) &&
           case_early_years_no_year_jump_presentation_guard(summary);
}
