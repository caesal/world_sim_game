#include "game_loop.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "render/plague_visual.h"
#include "sim/simulation_scheduler.h"

static DWORD last_frame_tick = 0;
static DWORD last_month_measure_tick = 0;
static int sim_accumulator_ms = 0;
static int actual_ms_per_month = 0;

void game_loop_reset(void) {
    last_frame_tick = GetTickCount();
    last_month_measure_tick = last_frame_tick;
    sim_accumulator_ms = 0;
    actual_ms_per_month = 0;
    sim_scheduler_reset();
}

static int sim_budget_ms_for_speed(void) {
    if (!auto_run) return 5;
    if (speed_index <= 0) return 5;
    if (speed_index == 1) return 11;
    return 22;
}

static void record_completed_months(DWORD now) {
    int completed = sim_scheduler_take_completed_months();
    int elapsed;
    int sample;

    if (completed <= 0) return;
    elapsed = clamp((int)(now - last_month_measure_tick), 1, 30000);
    last_month_measure_tick = now;
    sample = elapsed / completed;
    actual_ms_per_month = actual_ms_per_month <= 0 ? sample :
                          (actual_ms_per_month * 3 + sample) / 4;
}

int game_loop_tick_frame(void) {
    DWORD now = GetTickCount();
    int elapsed;
    int did_work = 0;
    int did_visual = 0;
    int requested = 0;

    if (last_frame_tick == 0) last_frame_tick = now;
    elapsed = (int)(now - last_frame_tick);
    last_frame_tick = now;
    elapsed = clamp(elapsed, 0, 250);

    if (auto_run) {
        int interval = SPEED_MS[clamp(speed_index, 0, 2)];
        sim_accumulator_ms += elapsed;
        while (sim_accumulator_ms >= interval && requested < 12) {
            sim_accumulator_ms -= interval;
            sim_scheduler_request_month();
            requested++;
        }
        if (requested >= 12 && sim_accumulator_ms > interval) sim_accumulator_ms = interval;
    } else {
        sim_accumulator_ms = 0;
    }

    if (sim_scheduler_has_pending_work()) {
        int budget_ms = sim_budget_ms_for_speed();
        DWORD start = GetTickCount();
        do {
            did_work |= sim_scheduler_run_budget(1);
        } while (sim_scheduler_has_pending_work() &&
                 (int)(GetTickCount() - start) < budget_ms);
    }
    record_completed_months(GetTickCount());
    did_visual = plague_visual_tick(elapsed);
    return did_work || did_visual || map_interaction_preview || dirty_any_render();
}

int game_loop_actual_ms_per_month(void) {
    return actual_ms_per_month;
}

int game_loop_pending_months(void) {
    return sim_scheduler_pending_months();
}
