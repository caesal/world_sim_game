#include "game/game_presentation_probe.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "game/game_loop.h"
#include "render/panel_alliance.h"
#include "render/panel_alliance_detail.h"
#include "render/panel_alliance_history.h"
#include "render/panel_alliance_model.h"
#include "render/panel_alliance_vote_state.h"
#include "render/panel_alliance_votes.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "sim/alliance.h"
#include "sim/civilization_slots.h"
#include "sim/simulation.h"
#include "sim/simulation_worker.h"
#include "ui/ui_map_display.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int panel_map_probe_alliance_legend_before(const RenderSnapshot *snapshot, int a_index, int b_index);

#define PRESENTATION_PROBE_DIR "build/validation/presentation_probe_20260618"

static AllianceSaveState blocking_alliance_state;
static AllianceSaveState stepped_alliance_state;

static void ensure_probe_dirs(void) {
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA(PRESENTATION_PROBE_DIR, NULL);
}

static int enqueue_month_sequence(int start_year, int start_month, int count) {
    int i;
    for (i = 0; i < count; i++) {
        int total = start_month - 1 + i;
        int y = start_year + total / 12;
        int m = total % 12 + 1;
        if (!simulation_worker_debug_enqueue_completed_month(y, m)) return 0;
    }
    return 1;
}

static int case_completed_month_queue(FILE *summary) {
    int y = 0, m = 0;
    int ok;
    simulation_worker_debug_reset_presentation_queue();
    ok = enqueue_month_sequence(42, 10, 4);
    ok &= simulation_worker_take_visual_month(&y, &m) && y == 42 && m == 10;
    ok &= simulation_worker_take_visual_month(&y, &m) && y == 42 && m == 11;
    ok &= simulation_worker_take_visual_month(&y, &m) && y == 42 && m == 12;
    ok &= simulation_worker_take_visual_month(&y, &m) && y == 43 && m == 1;
    ok &= !simulation_worker_take_visual_month(&y, &m);
    ok &= simulation_worker_visual_dropped_months() == 0;
    fprintf(summary, "case=completed_month_queue ok=%d max=%d shown=%d dropped=%d\n",
            ok, simulation_worker_visual_max_backlog(),
            simulation_worker_visual_presented_total(),
            simulation_worker_visual_dropped_months());
    return ok;
}

static int case_visual_backlog_throttle(FILE *summary) {
    int y = 0, m = 0;
    int ok;
    simulation_worker_debug_reset_presentation_queue();
    ok = enqueue_month_sequence(7, 1, 4);
    ok &= game_loop_presentation_backlog() == 4;
    ok &= simulation_worker_presentation_throttled();
    ok &= simulation_worker_take_visual_month(&y, &m) && y == 7 && m == 1;
    ok &= simulation_worker_take_visual_month(&y, &m) && y == 7 && m == 2;
    ok &= game_loop_presentation_backlog() == 2;
    ok &= !simulation_worker_presentation_throttled();
    ok &= simulation_worker_visual_dropped_months() == 0;
    fprintf(summary,
            "case=visual_backlog_throttle ok=%d backlog=%d max=%d dropped=%d throttled=%d\n",
            ok, game_loop_presentation_backlog(), game_loop_visual_max_backlog(),
            game_loop_visual_dropped_months(), simulation_worker_presentation_throttled());
    return ok;
}

static int case_bar_redraw_not_blocked(FILE *summary) {
    int flags1, flags2;
    int ok;
    auto_run = 0;
    world_generated = 0;
    speed_index = SPEED_COUNT - 1;
    year = 100;
    month = 1;
    game_loop_reset();
    dirty_reset_all();
    simulation_worker_debug_reset_presentation_queue();
    enqueue_month_sequence(100, 2, 50);
    auto_run = 1;
    flags1 = game_loop_tick_frame();
    flags2 = game_loop_tick_frame();
    auto_run = 0;
    simulation_worker_shutdown();
    ok = (flags1 & GAME_REDRAW_TOP_BAR) && (flags1 & GAME_REDRAW_BOTTOM_BAR) &&
         (flags2 & GAME_REDRAW_TOP_BAR) && (flags2 & GAME_REDRAW_BOTTOM_BAR) &&
         !(flags2 & GAME_REDRAW_SIDE_PANEL_DATA) &&
         game_loop_display_year() == 100 && game_loop_display_month() == 3 &&
         game_loop_visual_dropped_months() == 0 &&
         game_loop_displayed_month_order_skips() == 0;
    fprintf(summary,
            "case=bar_redraw_not_blocked ok=%d flags1=0x%02X flags2=0x%02X display=%d/%d backlog=%d dropped=%d order_skips=%d\n",
            ok, flags1, flags2, game_loop_display_year(), game_loop_display_month(),
            game_loop_presentation_backlog(), game_loop_visual_dropped_months(),
            game_loop_displayed_month_order_skips());
    return ok;
}

static void reset_alliance_probe_state(void) {
    AllianceSaveState *state;
    int i;
    simulation_reset_state();
    alliance_reset();
    civ_count = 1;
    world_generated = 1;
    civilization_reset_slot_state(0);
    civs[0].alive = 1;
    state = alliance_internal_state();
    memset(state, 0, sizeof(*state));
    state->next_id = 1;
    for (i = 0; i < 8; i++) {
        state->voluntary_cooldown[0][i] = 3 + i;
        state->kicked_cooldown[0][i] = 5 + i;
    }
}

static int cooldowns_match(const AllianceSaveState *a, const AllianceSaveState *b) {
    return memcmp(a->voluntary_cooldown, b->voluntary_cooldown, sizeof(a->voluntary_cooldown)) == 0 &&
           memcmp(a->kicked_cooldown, b->kicked_cooldown, sizeof(a->kicked_cooldown)) == 0;
}

static int case_alliance_year_step(FILE *summary) {
    AllianceYearWork work;
    int first_done;
    int steps = 0;
    reset_alliance_probe_state();
    alliance_update_year();
    alliance_copy_save_state(&blocking_alliance_state);
    reset_alliance_probe_state();
    alliance_year_work_begin(&work);
    first_done = alliance_update_year_step(&work, 1);
    while (!alliance_update_year_step(&work, 512) && steps < 4096) steps++;
    alliance_copy_save_state(&stepped_alliance_state);
    fprintf(summary, "case=alliance_year_step ok=%d first_done=%d steps=%d last_ms=%d peak_ms=%d\n",
            !first_done && cooldowns_match(&blocking_alliance_state, &stepped_alliance_state),
            first_done, steps,
            alliance_year_last_step_ms(), alliance_year_peak_step_ms());
    return !first_done && cooldowns_match(&blocking_alliance_state, &stepped_alliance_state);
}

static int case_map_display_alliance_tab(FILE *summary) {
    int original_mode = display_mode, original_language = ui_language;
    const char *country_tab, *alliance_tab;
    int ok = 1;
    display_mode = DISPLAY_POLITICAL;
    country_tab = ui_primary_panel_tab_label(UI_LANG_EN);
    ok &= strcmp(country_tab, "Country") == 0;
    display_mode = DISPLAY_ALLIANCE;
    alliance_tab = ui_primary_panel_tab_label(UI_LANG_EN);
    ok &= strcmp(alliance_tab, "Alliance") == 0;
    display_mode = DISPLAY_REGIONS;
    ok &= strcmp(ui_primary_panel_tab_label(UI_LANG_EN), "Country") == 0;
    display_mode = DISPLAY_ROUTE_POTENTIAL;
    ok &= strcmp(ui_primary_panel_tab_label(UI_LANG_EN), "Country") == 0;
    ok &= MAP_DISPLAY_MODE_COUNT == 6;
    ok &= MAP_DISPLAY_MODES[0] == DISPLAY_POLITICAL && MAP_DISPLAY_MODES[1] == DISPLAY_ALLIANCE;
    ok &= MAP_DISPLAY_MODES[2] == DISPLAY_GEOGRAPHY && MAP_DISPLAY_MODES[3] == DISPLAY_CLIMATE;
    ok &= MAP_DISPLAY_MODES[4] == DISPLAY_REGIONS && MAP_DISPLAY_MODES[5] == DISPLAY_ROUTE_POTENTIAL;
    ok &= strcmp(ui_map_display_label(0, UI_LANG_EN), "Country") == 0;
    ok &= strcmp(ui_map_display_label(1, UI_LANG_EN), "Alliance") == 0;
    ok &= strcmp(ui_map_display_label(2, UI_LANG_EN), "Geography") == 0;
    ok &= strcmp(ui_map_display_label(3, UI_LANG_EN), "Climate") == 0;
    ok &= strcmp(ui_map_display_label(4, UI_LANG_EN), "Province") == 0;
    ok &= strcmp(ui_map_display_label(5, UI_LANG_EN), "Routes") == 0;
    ok &= ALLIANCE_DETAIL_TAB_COUNT == 5;
    ok &= ALLIANCE_MEMBER_SORT_COUNT == 5;
    ok &= ALLIANCE_MEMBER_SORT_POPULATION == 0;
    ok &= ALLIANCE_MEMBER_SORT_PROVINCES == 1;
    ok &= ALLIANCE_MEMBER_SORT_ARMY == 2;
    ok &= ALLIANCE_MEMBER_SORT_TECHNOLOGY == 3;
    ok &= ALLIANCE_MEMBER_SORT_JOINED == 4;
    ui_language = UI_LANG_EN;
    ok &= strcmp(alliance_detail_tab_label(2), "Votes") == 0;
    ok &= strcmp(alliance_detail_vote_type_label(ALLIANCE_VOTE_CREATE), "Creation vote") == 0;
    ok &= strcmp(alliance_detail_vote_type_label(ALLIANCE_VOTE_JOIN), "Join vote") == 0;
    ok &= strcmp(alliance_detail_vote_type_label(ALLIANCE_VOTE_REMOVAL), "Removal vote") == 0;
    ok &= strcmp(alliance_detail_reason_label(ALLIANCE_REJECT_VOTE_FAILED), "vote failed") == 0;
    ui_language = UI_LANG_ZH;
    ok &= strcmp(alliance_detail_tab_label(1), "成员") == 0;
    ok &= strcmp(alliance_detail_vote_type_label(ALLIANCE_VOTE_CREATE), "创建投票") == 0;
    ok &= strcmp(alliance_detail_vote_type_label(ALLIANCE_VOTE_JOIN), "加入投票") == 0;
    ok &= strcmp(alliance_detail_vote_type_label(ALLIANCE_VOTE_REMOVAL), "清退投票") == 0;
    ok &= strcmp(alliance_detail_reason_label(ALLIANCE_REJECT_VOTE_FAILED), "投票未通过") == 0;
    ui_language = original_language;
    display_mode = original_mode;
    fprintf(summary, "case=map_display_alliance_tab ok=%d first_alliance=%s first_country=%s modes=%d\n",
            ok, alliance_tab, country_tab, MAP_DISPLAY_MODE_COUNT);
    return ok;
}

static int case_pressed_feedback_state(FILE *summary) {
    int ok = 1;
    ui_pressed_control_clear(NULL);
    ok &= !ui_pressed_control_is_active(UI_PRESSED_SPEED, SPEED_COUNT - 1);
    ui_pressed_control_set(NULL, UI_PRESSED_SPEED, SPEED_COUNT - 1);
    ok &= ui_pressed_control_is_active(UI_PRESSED_SPEED, SPEED_COUNT - 1);
    ok &= !ui_pressed_control_is_active(UI_PRESSED_MAP_MODE, 1);
    ui_pressed_control_set(NULL, UI_PRESSED_MAP_MODE, 1);
    ok &= ui_pressed_control_is_active(UI_PRESSED_MAP_MODE, 1);
    ok &= !ui_pressed_control_is_active(UI_PRESSED_SPEED, SPEED_COUNT - 1);
    ui_pressed_control_clear(NULL);
    ok &= !ui_pressed_control_is_active(UI_PRESSED_MAP_MODE, 1);
    fprintf(summary, "case=pressed_feedback_state ok=%d speed_index=%d map_mode_index=1\n",
            ok, SPEED_COUNT - 1);
    return ok;
}

static int case_alliance_panel_model(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    const AlliancePanelModel *model;
    const AlliancePanelRow *row;
    int original_language = ui_language;
    RECT probe_row = {10, 20, 410, 90}, type_a, war_a, type_b, war_b;
    int ok = snapshot != NULL;
    if (!snapshot) {
        fprintf(summary, "case=alliance_panel_model ok=0 reason=alloc\n");
        return 0;
    }
    snapshot->revision = 1;
    snapshot->alliance_revision = 11;
    snapshot->civs_revision = 22;
    snapshot->diplomacy_revision = 33;
    snapshot->year = 820;
    snapshot->civ_count = 5;
    snapshot->alliance_count = 1;
    snapshot->alliances[0].active = 1;
    snapshot->alliances[0].id = 2;
    snapshot->alliances[0].founder_civ_id = 1;
    snapshot->alliances[0].founded_year = 20;
    snapshot->alliances[0].member_count = 2;
    snapshot->alliances[0].members[0] = 1;
    snapshot->alliances[0].members[1] = 2;
    snapshot->alliances[0].joined_year_by_civ[1] = 20;
    snapshot->alliances[0].joined_year_by_civ[2] = 30;
    snapshot->civs[0].alive = 1;
    snapshot->civs[0].alliance_display_id = -1;
    snapshot->civs[0].summary.population = 50;
    snapshot->civs[0].treasury = 500;
    snapshot->civs[1].alive = 1;
    snapshot->civs[1].alliance_id = 2;
    snapshot->civs[1].alliance_display_id = 2;
    snapshot->civs[1].summary.population = 100;
    snapshot->civs[1].current_soldiers = 10;
    snapshot->civs[1].treasury = 1000;
    snapshot->civs[1].defensive_bloc_power = 11;
    snapshot->civs[1].tech_stage = 2;
    snapshot->civs[2].alive = 1;
    snapshot->civs[2].alliance_id = 2;
    snapshot->civs[2].alliance_display_id = 2;
    snapshot->civs[2].summary.population = 200;
    snapshot->civs[2].current_soldiers = 20;
    snapshot->civs[2].treasury = 2000;
    snapshot->civs[2].defensive_bloc_power = 22;
    snapshot->civs[2].tech_stage = 3;
    snapshot->civs[3].alive = 1;
    snapshot->civs[3].overlord = 1;
    snapshot->civs[3].alliance_id = -1;
    snapshot->civs[3].alliance_display_id = 2;
    snapshot->civs[3].summary.population = 25;
    snapshot->civs[3].current_soldiers = 5;
    snapshot->civs[3].treasury = 300;
    snapshot->civs[4].alive = 0;
    snapshot->civs[4].alliance_display_id = -1;
    snapshot->civs[4].summary.population = 400;
    snapshot->civs[4].treasury = 100;
    snapshot->wars[1][0].active = 1;
    model = alliance_panel_model_get(snapshot, 0, COUNTRY_SORT_POPULATION, 1);
    row = alliance_panel_model_find_alliance(model, 2);
    ok &= model && row;
    ok &= model->alliance_row_count == 1 && model->no_alliance_country_count == 1;
    ok &= row && row->member_count == 2 && row->vassal_count == 1;
    ok &= row && row->population == 325 && row->military == 35 && row->treasury == 3300 &&
          row->latest_join_year == 30 && row->tech_stage == 3;
    ok &= row && row->war_count == 1 && row->leader_civ == 1;
    ui_language = UI_LANG_EN;
    ok &= strcmp(alliance_panel_probe_sort_label(COUNTRY_SORT_POPULATION), "Population") == 0;
    ok &= strcmp(alliance_panel_probe_sort_label(COUNTRY_SORT_PROVINCES), "Provinces") == 0;
    ok &= strcmp(alliance_panel_probe_sort_label(COUNTRY_SORT_ARMY), "Army") == 0;
    ok &= strcmp(alliance_panel_probe_sort_label(COUNTRY_SORT_TREASURY), "Treasury") == 0;
    ok &= strcmp(alliance_panel_probe_sort_label(COUNTRY_SORT_TECH), "Technology") == 0;
    ok &= strcmp(alliance_panel_probe_sort_label(COUNTRY_SORT_DISORDER), "Members") == 0;
    ok &= alliance_panel_probe_country_row_badge_count() == 2 && alliance_panel_probe_no_alliance_badge_color() == RGB(128, 128, 128);
    alliance_panel_probe_badge_rects(probe_row, &type_a, &war_a);
    alliance_panel_probe_badge_rects(probe_row, &type_b, &war_b);
    ok &= EqualRect(&type_a, &type_b) && EqualRect(&war_a, &war_b);
    model = alliance_panel_model_get(snapshot, 1, COUNTRY_SORT_POPULATION, 1);
    ok &= model && model->show_fallen == 1 && model->alliance_row_count == 0 &&
          model->no_alliance_country_count == 1 && model->rows[0].civ_id == 4;
    model = alliance_panel_model_get(snapshot, 0, COUNTRY_SORT_TREASURY, 0);
    ok &= model && model->sort_column == COUNTRY_SORT_TREASURY && model->sort_descending == 0;
    snapshot->alliance_count = 2; snapshot->alliances[1] = snapshot->alliances[0];
    snapshot->alliances[1].id = 3; snapshot->alliances[1].members[0] = 2;
    snapshot->alliances[1].members[1] = 3; snapshot->civs[3].current_soldiers = 25;
    ok &= panel_map_probe_alliance_legend_before(snapshot, 1, 0);
    ui_language = original_language;
    fprintf(summary,
            "case=alliance_panel_model ok=%d rows=%d alliances=%d no_alliance=%d pop=%d military=%d wars=%d leader=%d\n",
            ok, model ? model->row_count : -1, model ? model->alliance_row_count : -1,
            model ? model->no_alliance_country_count : -1, row ? row->population : -1,
            row ? row->military : -1, row ? row->war_count : -1, row ? row->leader_civ : -1);
    free(snapshot);
    return ok;
}

static void init_alliance_ui_probe_snapshot(RenderSnapshot *snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->year = 122;
    snapshot->civ_count = 3;
    snapshot->alliance_count = 1;
    snapshot->alliances[0].active = 1;
    snapshot->alliances[0].id = 7;
    snapshot->alliances[0].founded_year = 90;
    snapshot->alliances[0].member_count = 2;
    snapshot->alliances[0].members[0] = 1;
    snapshot->alliances[0].members[1] = 2;
    snapshot->alliances[0].joined_year_by_civ[1] = 90;
    snapshot->alliances[0].joined_year_by_civ[2] = 121;
    snprintf(snapshot->alliances[0].name_en, sizeof(snapshot->alliances[0].name_en), "Probe Alliance");
    snprintf(snapshot->alliances[0].name_zh, sizeof(snapshot->alliances[0].name_zh), "探针联盟");
    snapshot->civs[0].alive = 1;
    snapshot->civs[0].symbol = 'C';
    snprintf(snapshot->civs[0].name_en, sizeof(snapshot->civs[0].name_en), "Candidate");
    snprintf(snapshot->civs[0].name_zh, sizeof(snapshot->civs[0].name_zh), "候选国");
    snapshot->civs[1].alive = 1;
    snapshot->civs[1].symbol = 'A';
    snprintf(snapshot->civs[1].name_en, sizeof(snapshot->civs[1].name_en), "Member A");
    snprintf(snapshot->civs[1].name_zh, sizeof(snapshot->civs[1].name_zh), "成员甲");
    snapshot->civs[2].alive = 1;
    snapshot->civs[2].symbol = 'B';
    snprintf(snapshot->civs[2].name_en, sizeof(snapshot->civs[2].name_en), "Member B");
    snprintf(snapshot->civs[2].name_zh, sizeof(snapshot->civs[2].name_zh), "成员乙");
}

static void init_vote_record(AllianceVoteRecord *vote, int year) {
    int i;
    memset(vote, 0, sizeof(*vote));
    vote->active = 1;
    vote->alliance_id = 7;
    vote->vote_year = year;
    vote->vote_type = ALLIANCE_VOTE_JOIN;
    vote->target_civ_id = 0;
    vote->yes_count = 1;
    vote->no_count = 1;
    vote->passed = 0;
    vote->rejection_reason = ALLIANCE_REJECT_VOTE_FAILED;
    for (i = 0; i < MAX_CIVS; i++) vote->member_votes[i] = ALLIANCE_MEMBER_VOTE_NA;
    vote->member_votes[1] = ALLIANCE_MEMBER_VOTE_YES;
    vote->member_votes[2] = ALLIANCE_MEMBER_VOTE_NO;
}

static AllianceCandidateRecord make_candidate(int candidate_year, int progress,
                                              int status, int reason, int updated_year) {
    AllianceCandidateRecord candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.active = 1;
    candidate.alliance_id = 7;
    candidate.civ_id = 0;
    candidate.type = ALLIANCE_CANDIDATE_JOIN;
    candidate.initiated_by = ALLIANCE_CANDIDATE_INITIATOR_CANDIDATE;
    candidate.candidate_year = candidate_year;
    candidate.qualification_progress = progress;
    candidate.status = status;
    candidate.rejection_reason = reason;
    candidate.updated_year = updated_year;
    return candidate;
}

static int case_alliance_vote_history_ui_semantics(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    AllianceCandidateRecord first, voting, retry, recent, old, passed;
    AllianceVoteRecord vote;
    AllianceHistoryRecord history;
    char sentence[320];
    int original_language = ui_language;
    int remaining = -1, total = -1, progress = -1;
    int retry_remaining = -1, retry_total = -1, retry_progress = -1;
    const char *first_label, *voting_label, *retry_label, *not_passed_label, *retry_without_vote_label;
    int visible_recent, visible_old, visible_passed, symbols_ok, history_ok, order_ok, tooltip_ok, voter_ok;
    int ok = snapshot != NULL;
    if (!snapshot) {
        fprintf(summary, "case=alliance_vote_history_ui_semantics ok=0 reason=alloc\n");
        return 0;
    }
    init_alliance_ui_probe_snapshot(snapshot);
    init_vote_record(&vote, 120);
    first = make_candidate(112, 25, ALLIANCE_CANDIDATE_ACTIVE,
                           ALLIANCE_REJECT_NONE, 112);
    voting = make_candidate(93, 100, ALLIANCE_CANDIDATE_ACTIVE,
                            ALLIANCE_REJECT_NONE, 122);
    retry = make_candidate(100, 100, ALLIANCE_CANDIDATE_ACTIVE,
                           ALLIANCE_REJECT_VOTE_FAILED, 120);
    passed = make_candidate(60, 100, ALLIANCE_CANDIDATE_PASSED,
                            ALLIANCE_REJECT_NONE, 72);
    recent = make_candidate(60, 100, ALLIANCE_CANDIDATE_REJECTED,
                            ALLIANCE_REJECT_VOTE_FAILED, 72);
    old = make_candidate(60, 100, ALLIANCE_CANDIDATE_REJECTED,
                         ALLIANCE_REJECT_VOTE_FAILED, 71);
    ui_language = UI_LANG_EN;
    first_label = alliance_votes_probe_candidate_status_label(snapshot, &first, NULL);
    voting_label = alliance_votes_probe_candidate_status_label(snapshot, &voting, NULL);
    snapshot->year = 121;
    not_passed_label = alliance_votes_probe_candidate_status_label(snapshot, &retry, &vote);
    snapshot->year = 122;
    retry_label = alliance_votes_probe_candidate_status_label(snapshot, &retry, &vote);
    retry_without_vote_label = alliance_votes_probe_candidate_status_label(snapshot, &retry, NULL);
    progress = alliance_votes_probe_phase_progress(snapshot, &first, NULL, &remaining, &total);
    retry_progress = alliance_votes_probe_phase_progress(snapshot, &retry, &vote,
                                                        &retry_remaining, &retry_total);
    visible_recent = alliance_votes_probe_candidate_visible(snapshot, &recent);
    visible_old = alliance_votes_probe_candidate_visible(snapshot, &old);
    visible_passed = alliance_vote_state_visible(snapshot, &snapshot->alliances[0], &passed);
    order_ok = alliance_vote_state_candidate_before(snapshot, &snapshot->alliances[0], &first, &passed);
    voter_ok = alliance_vote_state_vote_member_count(&snapshot->alliances[0], &vote) == 1 &&
               !alliance_vote_state_member_can_vote(&snapshot->alliances[0], 2, 0, vote.vote_year);
    diplomacy_score_tooltip_begin();
    diplomacy_score_tooltip_register_bar((RECT){10, 10, 110, 22}, 0, 1);
    diplomacy_score_tooltip_commit();
    tooltip_ok = diplomacy_score_tooltip_hover_key(20, 16) > 0;
    symbols_ok = strcmp(alliance_votes_probe_vote_symbol(ALLIANCE_MEMBER_VOTE_NO), "X") == 0 &&
                 strcmp(alliance_votes_probe_vote_symbol(ALLIANCE_MEMBER_VOTE_YES),
                        alliance_votes_probe_vote_symbol(ALLIANCE_MEMBER_VOTE_NA)) != 0 &&
                 strcmp(alliance_votes_probe_vote_symbol(ALLIANCE_MEMBER_VOTE_ABSTAIN),
                        alliance_votes_probe_vote_symbol(ALLIANCE_MEMBER_VOTE_NA)) != 0;
    memset(&history, 0, sizeof(history));
    history.active = 1;
    history.alliance_id = 7;
    history.event_year = 122;
    history.event_type = ALLIANCE_HISTORY_VOTE_FAILED;
    history.civ_id = 0;
    history.target_civ_id = -1;
    history.vote_type = ALLIANCE_VOTE_JOIN;
    history.rejection_reason = ALLIANCE_REJECT_VOTE_FAILED;
    alliance_history_probe_sentence(sentence, sizeof(sentence), snapshot, &history);
    history_ok = strstr(sentence, " / ") == NULL && strstr(sentence, " -") == NULL &&
                 strstr(sentence, "waiting for the next vote") != NULL;
    ok &= strcmp(first_label, "First vote countdown") == 0 && progress == 36 &&
          remaining == 19 && total == ALLIANCE_JOIN_FIRST_VOTE_YEARS;
    ok &= strcmp(voting_label, "Waiting for vote result") == 0;
    ok &= strcmp(not_passed_label, "Not passed") == 0;
    ok &= strcmp(retry_label, "Waiting for next vote") == 0 &&
          retry_progress == 20 && retry_remaining == 8 &&
          retry_total == ALLIANCE_JOIN_RETRY_VOTE_YEARS;
    ok &= strcmp(retry_without_vote_label, "Waiting for next vote") != 0;
    ok &= visible_recent && visible_passed && !visible_old && order_ok &&
          tooltip_ok && symbols_ok && history_ok && voter_ok;
    ui_language = original_language;
    fprintf(summary,
            "case=alliance_vote_history_ui_semantics ok=%d first=%s progress=%d rem=%d/%d vote_year=%s not_passed=%s retry=%s retry_progress=%d rem=%d/%d recent=%d passed_recent=%d old=%d order=%d voters=%d tooltip=%d symbols=%d history_ok=%d sentence=\"%s\"\n",
            ok, first_label, progress, remaining, total, voting_label, not_passed_label,
            retry_label, retry_progress, retry_remaining, retry_total, visible_recent,
            visible_passed, visible_old, order_ok, voter_ok, tooltip_ok, symbols_ok, history_ok, sentence);
    free(snapshot);
    return ok;
}

int run_presentation_probe(void) {
    FILE *summary;
    int ok = 1;
    ensure_probe_dirs();
    summary = fopen(PRESENTATION_PROBE_DIR "/summary.txt", "w");
    if (!summary) return 2;
    ok &= case_completed_month_queue(summary);
    ok &= case_visual_backlog_throttle(summary);
    ok &= case_bar_redraw_not_blocked(summary);
    ok &= case_alliance_year_step(summary);
    ok &= case_map_display_alliance_tab(summary);
    ok &= case_pressed_feedback_state(summary);
    ok &= case_alliance_panel_model(summary);
    ok &= case_alliance_vote_history_ui_semantics(summary);
    fprintf(summary, "overall_ok=%d\n", ok);
    fclose(summary);
    printf("presentation probe summary: %s\\summary.txt\n", PRESENTATION_PROBE_DIR);
    return ok ? 0 : 1;
}
