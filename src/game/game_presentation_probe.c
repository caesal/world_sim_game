#include "game/game_presentation_probe.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "game/game_loop.h"
#include "sim/alliance.h"
#include "sim/civilization_slots.h"
#include "sim/simulation.h"
#include "sim/simulation_worker.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

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
    fprintf(summary, "overall_ok=%d\n", ok);
    fclose(summary);
    printf("presentation probe summary: %s\\summary.txt\n", PRESENTATION_PROBE_DIR);
    return ok ? 0 : 1;
}
